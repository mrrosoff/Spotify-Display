#!/usr/bin/env python

import os
import sys
import threading
import time
from datetime import datetime, timedelta
from io import BytesIO

from PIL import Image, UnidentifiedImageError
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics
import spotipy
from spotipy.oauth2 import SpotifyOAuth
import requests
from requests import RequestException

import weather

FONTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts")
CA_BUNDLE = "/etc/ssl/certs/ca-certificates.crt"

IDLE_TIMEOUT = 5 * 60          # switch to weather after 5 min with no track
POLL_INTERVAL = 1.0
HTTP_TIMEOUT = (5, 10)

WEATHER_TTL = 1800
WEATHER_RETRY_TTL = 120
WEATHER_REDRAW = 30            # repaint weather view every 30s

NIGHT_START_MIN = 20 * 60 + 30  # 8:30 PM -> show tomorrow's weather
NIGHT_END_MIN = 7 * 60          # 7:00 AM

DAY_BRIGHTNESS = 100
NIGHT_BRIGHTNESS = 50


weather_cache = {"data": None, "last_fetch": 0.0, "day_index": None}
weather_lock = threading.Lock()


def log(msg):
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{ts}] {msg}", flush=True)


def load_font(name):
    f = graphics.Font()
    f.LoadFont(os.path.join(FONTS_DIR, name))
    return f


def rgb(r, g, b):
    return graphics.Color(r, g, b)


GREY = rgb(100, 100, 100)
DIM = rgb(40, 40, 40)
LABEL = rgb(200, 200, 200)
YELLOW = rgb(255, 200, 0)
ICON_COLOR = rgb(180, 200, 230)
PRECIP_BLUE = rgb(120, 170, 220)


def text_width(font, s):
    return sum(font.CharacterWidth(ord(c)) for c in s)


def draw_text_top(canvas, font, x, top_y, color, text):
    graphics.DrawText(canvas, font, x, top_y + font.baseline, color, text)


def draw_text_centered(canvas, font, cx, cy, color, text):
    w = text_width(font, text)
    top = cy - font.height // 2
    graphics.DrawText(canvas, font, cx - w // 2, top + font.baseline, color, text)


def draw_icon(canvas, pixels, x0, y0, color):
    for y, row in enumerate(pixels):
        for x, on in enumerate(row):
            if on:
                canvas.SetPixel(x0 + x, y0 + y, color.red, color.green, color.blue)


def is_night():
    m = datetime.now().hour * 60 + datetime.now().minute
    return m >= NIGHT_START_MIN or m < NIGHT_END_MIN


def want_tomorrow():
    return is_night()


def refresh_weather(day_index):
    try:
        data = weather.fetch_forecast(CA_BUNDLE, day_index=day_index)
        with weather_lock:
            weather_cache["data"] = data
            weather_cache["last_fetch"] = time.time()
            weather_cache["day_index"] = day_index
        log(f"weather day={day_index}: code={data['code']} hi={data['hi']} "
            f"lo={data['lo']} precip={data['precip']}")
    except (RequestException, ValueError, KeyError) as e:
        with weather_lock:
            weather_cache["last_fetch"] = time.time() - (WEATHER_TTL - WEATHER_RETRY_TTL)
        log(f"weather FAILED: {type(e).__name__}: {e}")


def weather_loop():
    while True:
        desired_day = 1 if want_tomorrow() else 0
        with weather_lock:
            age = time.time() - weather_cache["last_fetch"]
            cached_day = weather_cache["day_index"]
        if cached_day != desired_day or age >= WEATHER_TTL:
            refresh_weather(desired_day)
        time.sleep(30)


def render_weather_view(canvas, fonts, icons):
    canvas.Clear()
    title_font = fonts["title"]
    row_font = fonts["row"]

    with weather_lock:
        data = weather_cache["data"]
        day_index = weather_cache["day_index"]

    when = datetime.now() + timedelta(days=day_index or 0)
    header = when.strftime("%b %d").upper()
    draw_text_centered(canvas, title_font, 32, 6, GREY, header)
    graphics.DrawLine(canvas, 0, 11, 63, 11, DIM)

    if data is None:
        draw_text_centered(canvas, row_font, 32, 38, LABEL, "Loading...")
        return

    icon_name = weather.CODE_ICON.get(data["code"], "cloud")
    word = weather.CODE_WORD.get(data["code"], "")
    _, _, pixels = icons.get(icon_name, icons["cloud"])
    draw_icon(canvas, pixels, 6, 17, ICON_COLOR)

    rx = 41
    draw_text_top(canvas, title_font, rx, 17, YELLOW, f"{data['hi']}°")
    draw_text_top(canvas, row_font, rx, 29, LABEL, f"{data['lo']}°")
    draw_text_top(canvas, row_font, rx, 39, PRECIP_BLUE, f"{data['precip']}%")
    draw_text_centered(canvas, row_font, 32, 55, LABEL, word.upper())


def fetch_album_image(url):
    r = requests.get(url, timeout=HTTP_TIMEOUT)
    r.raise_for_status()
    return Image.open(BytesIO(r.content)).convert("RGB")


def main():
    # Load all assets from disk before constructing RGBMatrix, which drops
    # root privileges and loses access to files under /home/user (mode 700).
    fonts = {
        "title": load_font("6x10.bdf"),
        "row": load_font("5x7.bdf"),
    }
    icons = weather.load_icons()

    # Force PIL to import its format plugins (JPEG/PNG/etc.) now, before
    # RGBMatrix() drops privileges. Otherwise those imports happen lazily
    # on first Image.open(), and the post-drop daemon user can't traverse
    # /home/user to reach the plugin modules.
    Image.init()

    options = RGBMatrixOptions()
    options.rows = 64
    options.cols = 64
    options.chain_length = 1
    options.parallel = 1
    options.pwm_dither_bits = 1
    options.limit_refresh_rate_hz = 120
    options.hardware_mapping = "regular"
    matrix = RGBMatrix(options=options)

    loading_canvas = matrix.CreateFrameCanvas()
    draw_text_centered(loading_canvas, fonts["row"], 32, 32, LABEL, "Loading...")
    matrix.SwapOnVSync(loading_canvas)

    sp = spotipy.Spotify(
        auth_manager=SpotifyOAuth(
            client_id="a9a84f65fc9f47568870f4c0c0185e3a",
            client_secret="7cb7fe064e1844c19e87a2d475573948",
            redirect_uri="http://127.0.0.1/callback",
            scope="user-read-currently-playing",
            open_browser=False,
            cache_path="/home/root/cache.json",
        )
    )

    threading.Thread(target=weather_loop, daemon=True).start()

    canvas = matrix.CreateFrameCanvas()
    prev_url = None
    last_playing = 0.0
    last_weather_draw = 0.0
    mode = None
    current_brightness = None

    while True:
        target_brightness = NIGHT_BRIGHTNESS if is_night() else DAY_BRIGHTNESS
        if target_brightness != current_brightness:
            matrix.brightness = target_brightness
            current_brightness = target_brightness
            log(f"brightness -> {target_brightness}")
            # Force a redraw of whatever mode we're in so the new brightness
            # takes effect immediately.
            prev_url = None
            mode = None


        try:
            info = sp.current_user_playing_track()
        except (RequestException, spotipy.SpotifyException) as e:
            log(f"spotify poll failed: {type(e).__name__}: {e}")
            info = None
        except Exception as e:
            log(f"spotify poll unexpected: {type(e).__name__}: {e}")
            info = None

        url = None
        item = info.get("item") if info is not None else None
        if item is not None:
            images = item.get("album", {}).get("images") or item.get("images") or []
            if images:
                url = images[-1]["url"]
            else:
                log(f"no images on item type={item.get('type')} name={item.get('name')}")

        now = time.time()

        if url is not None:
            last_playing = now
            if url != prev_url or mode != "art":
                try:
                    im = fetch_album_image(url)
                except (RequestException, UnidentifiedImageError, OSError) as e:
                    log(f"art fetch failed: {type(e).__name__}: {e} url={url}")
                else:
                    matrix.SetImage(im)
                    prev_url = url
                    mode = "art"
                    log(f"show art: {url}")
        else:
            idle = now - last_playing
            if idle >= IDLE_TIMEOUT:
                if mode != "weather" or (now - last_weather_draw) >= WEATHER_REDRAW:
                    render_weather_view(canvas, fonts, icons)
                    canvas = matrix.SwapOnVSync(canvas)
                    last_weather_draw = now
                    if mode != "weather":
                        log(f"idle {int(idle)}s -> weather")
                    mode = "weather"
                    prev_url = None

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
