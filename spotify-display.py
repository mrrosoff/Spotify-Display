#!/usr/bin/env python

import os
import sys
import threading
import time
from datetime import datetime, timedelta

from PIL import Image, ImageDraw
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics
import spotipy
from spotipy.oauth2 import SpotifyOAuth
import requests
from requests import RequestException

import weather

FONTS_DIR = os.path.join(os.path.dirname(__file__), "fonts")
CA_BUNDLE = "/etc/ssl/certs/ca-certificates.crt"

IDLE_TIMEOUT = 5 * 60          # switch to weather after 5 min with no track
POLL_INTERVAL = 1.0
HTTP_TIMEOUT = (5, 10)

WEATHER_TTL = 1800
WEATHER_RETRY_TTL = 120
WEATHER_REDRAW = 30            # repaint weather view every 30s

NIGHT_START_MIN = 20 * 60 + 30  # 8:30 PM -> show tomorrow's weather
NIGHT_END_MIN = 7 * 60          # 7:00 AM

SPOTIFY_GREEN = (30, 215, 96)


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


def make_spotify_logo(size=32):
    """Render a simplified Spotify glyph: green circle with three black arcs."""
    img = Image.new("RGB", (size, size), (0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([0, 0, size - 1, size - 1], fill=SPOTIFY_GREEN)
    # (top_y_frac, width_frac, height_frac, thickness)
    arcs = [
        (0.18, 0.72, 0.40, 3),
        (0.34, 0.58, 0.34, 2),
        (0.50, 0.44, 0.26, 2),
    ]
    for top_f, w_f, h_f, thick in arcs:
        w = int(size * w_f)
        h = int(size * h_f)
        x0 = (size - w) // 2
        y0 = int(size * top_f)
        d.arc([x0, y0, x0 + w, y0 + h],
              start=180, end=360, fill=(0, 0, 0), width=thick)
    return img


def paint_image(canvas, img, x0, y0):
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if r or g or b:
                canvas.SetPixel(x0 + x, y0 + y, r, g, b)


def render_loading(canvas, fonts, logo_img, dots):
    canvas.Clear()
    lw, _ = logo_img.size
    paint_image(canvas, logo_img, (64 - lw) // 2, 8)
    text = "Loading" + "." * dots + " " * (3 - dots)
    draw_text_centered(canvas, fonts["row"], 32, 52, LABEL, text)


def loading_animator(matrix, fonts, logo_img, stop_event):
    canvas = matrix.CreateFrameCanvas()
    i = 0
    while not stop_event.is_set():
        render_loading(canvas, fonts, logo_img, i % 4)
        canvas = matrix.SwapOnVSync(canvas)
        if stop_event.wait(0.4):
            break
        i += 1


def want_tomorrow():
    m = datetime.now().hour * 60 + datetime.now().minute
    return m >= NIGHT_START_MIN or m < NIGHT_END_MIN


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
    with requests.get(url, stream=True, timeout=HTTP_TIMEOUT) as r:
        r.raise_for_status()
        return Image.open(r.raw).convert("RGB").copy()


def main():
    options = RGBMatrixOptions()
    options.rows = 64
    options.cols = 64
    options.chain_length = 1
    options.parallel = 1
    options.pwm_dither_bits = 1
    options.limit_refresh_rate_hz = 120
    options.hardware_mapping = "regular"
    matrix = RGBMatrix(options=options)

    fonts = {
        "title": load_font("6x10.bdf"),
        "row": load_font("5x7.bdf"),
    }
    icons = weather.load_icons()
    logo_img = make_spotify_logo(32)

    stop_loading = threading.Event()
    anim_thread = threading.Thread(
        target=loading_animator, args=(matrix, fonts, logo_img, stop_loading),
        daemon=True,
    )
    anim_thread.start()

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
    last_playing = time.time()
    last_weather_draw = 0.0
    mode = "loading"

    def stop_loading_anim():
        nonlocal mode
        if mode == "loading":
            stop_loading.set()
            anim_thread.join()

    while True:
        try:
            info = sp.current_user_playing_track()
        except (RequestException, spotipy.SpotifyException) as e:
            log(f"spotify poll failed: {type(e).__name__}: {e}")
            info = None

        url = None
        if info is not None and info.get("item") is not None:
            images = info["item"]["album"]["images"]
            if images:
                url = images[-1]["url"]

        now = time.time()

        if url is not None:
            last_playing = now
            if url != prev_url or mode != "art":
                try:
                    im = fetch_album_image(url)
                except RequestException as e:
                    log(f"art fetch failed: {type(e).__name__}: {e}")
                else:
                    stop_loading_anim()
                    matrix.SetImage(im)
                    prev_url = url
                    mode = "art"
                    log(f"show art: {url}")
        else:
            idle = now - last_playing
            if idle >= IDLE_TIMEOUT:
                stop_loading_anim()
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
