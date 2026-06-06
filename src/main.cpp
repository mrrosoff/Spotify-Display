#include "core/colors.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "data/caches.hpp"
#include "data/fetchers.hpp"
#include "data/weather_codes.hpp"
#include "net/http.hpp"
#include "net/spotify.hpp"
#include "render/draw.hpp"
#include "render/fonts.hpp"
#include "render/render.hpp"
#include "util/image.hpp"
#include "util/time_utils.hpp"
#include "util/xbm.hpp"

#include "led-matrix.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>

using namespace std;
using logx::log;

atomic<bool> g_interrupted{false};

namespace {

void on_signal(int) {
    g_interrupted.store(true);
}

void boot_grace() {
    ifstream f("/proc/uptime");
    if (!f) return;
    double up = 0;
    f >> up;
    const double delay = cfg::BOOT_GRACE.count() - up;
    if (delay > 0) {
        log("boot grace: sleeping ", static_cast<int>(delay), "s before matrix init");
        this_thread::sleep_for(chrono::duration<double>(delay));
    }
}

map<string, XbmIcon> load_all_icons() {
    map<string, XbmIcon> icons;
    for (const auto &[code, name] : CODE_ICON) {
        if (icons.count(name)) continue;
        XbmIcon ic;
        const string path = "icons/" + name + ".xbm";
        if (load_xbm(path, &ic))
            icons[name] = move(ic);
        else
            log("icon FAILED: ", path);
    }
    log("loaded ", icons.size(), " weather icons");
    return icons;
}

unique_ptr<rgb_matrix::RGBMatrix> create_matrix() {
    rgb_matrix::RGBMatrix::Options options;
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = cfg::DAY_BRIGHTNESS;
    // Tuned on the Pi Zero for a steady, flicker-free panel (measured):
    options.pwm_lsb_nanoseconds = 70;     // base bit-time; low value keeps raw refresh well above the 120 cap
    options.limit_refresh_rate_hz = 120;  // cap below the raw ceiling -> uniform frames, no swap-induced dips
    options.disable_busy_waiting = true;  // nanosleep vs spin: frees the single core for the fetcher thread

    rgb_matrix::RuntimeOptions runtime;
    runtime.gpio_slowdown = 1;            // library default; slowdown=0 caused panel ghosting on the Pi Zero

    return unique_ptr<rgb_matrix::RGBMatrix>(
        rgb_matrix::RGBMatrix::CreateFromOptions(options, runtime)
    );
}

enum class Mode { None, Art, Weather };

}  // namespace

int main() {
    // Curl can otherwise deliver SIGPIPE on a broken connection and kill the
    // process. We dropped CURLOPT_NOSIGNAL so curl uses SIGALRM for timeouts;
    // this is the matching half that keeps SIGPIPE from being fatal.
    signal(SIGPIPE, SIG_IGN);

    boot_grace();
    http::global_init();

    if (!spotify::init()) {
        log("warning: spotify init incomplete — playback polling will fail");
    }

    Fonts fonts;
    if (!fonts.load()) {
        cerr << "font load failed\n";
        return 1;
    }
    auto icons = load_all_icons();

    auto matrix = create_matrix();
    if (!matrix) {
        cerr << "matrix init failed\n";
        return 1;
    }
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    auto *loading_canvas = matrix->CreateFrameCanvas();
    render::loading(loading_canvas, fonts);
    matrix->SwapOnVSync(loading_canvas);

    auto *canvas = matrix->CreateFrameCanvas();
    string prev_art_url;
    bool prev_paused = false;
    img::Bitmap art_buf;  // last decoded album-art bitmap, re-blittable on pause toggle
    http::Session art_session;  // reused across art fetches (Spotify CDN keep-alive)
    double last_playing = 0.0;
    double last_weather_draw = 0.0;
    Mode mode = Mode::None;
    int current_brightness = -1;
    bool prev_night = tu::is_night();

    constexpr double PAUSED_DIM = 0.5;
    constexpr int PAUSE_PADDING = 3;
    constexpr int WIPE_STEP = 2;  // columns per frame; 64/2 = 32 frames

    // Day/night base brightness, from the clock.
    const auto base_brightness = [] {
        return tu::is_night() ? cfg::NIGHT_BRIGHTNESS : cfg::DAY_BRIGHTNESS;
    };
    // Drive all dimming through the library's CIE-mapped SetBrightness rather
    // than scaling RGB in software: the library scales once through its
    // perceptual curve, avoiding the truncation/banding a raw 8-bit multiply
    // causes in dark album art. SetBrightness applies to pixels written after
    // the call, so set it before drawing.
    const auto set_brightness = [&](int b) {
        if (b == current_brightness) return;
        matrix->SetBrightness(b);
        current_brightness = b;
        log("brightness -> ", b);
    };

    const auto blit_art = [&](bool paused) {
        set_brightness(
            paused ? static_cast<int>(base_brightness() * PAUSED_DIM)
                   : base_brightness()
        );
        canvas->Clear();
        img::draw(art_buf, canvas, 0, 0);
        if (paused) {
            draw::pause_icon(canvas, 63 - PAUSE_PADDING, 63 - PAUSE_PADDING);
        }
        canvas = matrix->SwapOnVSync(canvas);
    };

    // Horizontal column wipe from the currently displayed `art_buf` to `next`.
    // After the wipe completes, the caller should move `next` into `art_buf`.
    const auto wipe_art = [&](const img::Bitmap &next) {
        set_brightness(base_brightness());  // wipe is a playing transition
        for (int split = WIPE_STEP; split < 64; split += WIPE_STEP) {
            if (g_interrupted.load()) return;
            canvas->Clear();
            img::draw_split(art_buf, next, canvas, 0, 0, split);
            canvas = matrix->SwapOnVSync(canvas);
        }
    };

    while (!g_interrupted.load()) {
        const bool night = tu::is_night();
        if (night != prev_night) {
            prev_night = night;
            log("day/night -> ", night ? "night" : "day");
            // Force a redraw so the new base brightness takes effect on screen;
            // the draw paths apply it via set_brightness.
            prev_art_url.clear();
            mode = Mode::None;
        }

        spotify::NowPlaying np;
        string err;
        const bool ok = spotify::current_playback(&np, &err);
        if (!ok) {
            log("spotify poll failed: ", err);
        }

        // Serialize all network work on one thread — weather only ticks
        // when its TTL has elapsed, so this is a cheap no-op most iterations.
        fetch::weather_tick();

        const double now = static_cast<double>(tu::now_unix());

        if (np.active && !np.album_art_url.empty()) {
            last_playing = now;
            const bool url_changed = np.album_art_url != prev_art_url;
            const bool pause_changed = np.paused != prev_paused;
            const bool first_art = art_buf.rgb.empty() || mode != Mode::Art;
            const bool need_fetch = url_changed || first_art;
            if (need_fetch) {
                string body, herr;
                if (!art_session.get(
                        np.album_art_url,
                        cfg::CONNECT_TIMEOUT_S,
                        cfg::READ_TIMEOUT_S,
                        &body,
                        &herr
                    )) {
                    // Server-status errors leave the pool healthy; anything
                    // else is transport, so recycle.
                    if (herr.rfind("HTTP ", 0) != 0) art_session.reset();
                    log("art fetch failed: ", herr, " url=", np.album_art_url);
                } else {
                    img::Bitmap fresh;
                    string derr;
                    if (!img::decode(body, 64, 64, &fresh, &derr)) {
                        log("art decode failed: ", derr);
                    } else {
                        if (first_art) {
                            art_buf = move(fresh);
                            blit_art(np.paused);
                        } else {
                            wipe_art(fresh);
                            art_buf = move(fresh);
                            blit_art(np.paused);
                        }
                        log("show art: ", np.album_art_url);
                        prev_art_url = np.album_art_url;
                        prev_paused = np.paused;
                        mode = Mode::Art;
                    }
                }
            } else if (pause_changed) {
                blit_art(np.paused);
                prev_paused = np.paused;
                log(np.paused ? "paused" : "resumed");
            }
        } else {
            // Treat Spotify network failures the same as "nothing playing" —
            // fall through to weather so the display isn't stuck on "Loading"
            // when the API is unreachable.
            const double idle = now - last_playing;
            if (idle >= cfg::IDLE_TIMEOUT.count()) {
                const bool need_redraw =
                    mode != Mode::Weather ||
                    (now - last_weather_draw) >= cfg::WEATHER_REDRAW.count();
                if (need_redraw) {
                    set_brightness(base_brightness());
                    render::weather(canvas, fonts, icons);
                    canvas = matrix->SwapOnVSync(canvas);
                    last_weather_draw = now;
                    if (mode != Mode::Weather) {
                        log("idle ", static_cast<int>(idle), "s -> weather");
                    }
                    mode = Mode::Weather;
                    prev_art_url.clear();
                    art_buf = {};
                }
            }
        }

        this_thread::sleep_for(cfg::POLL_INTERVAL);
    }

    matrix->Clear();
    matrix.reset();
    http::global_cleanup();
    return 0;
}
