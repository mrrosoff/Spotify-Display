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

namespace fetch {
atomic<bool> g_interrupted{false};  // shared with fetchers.cpp
}
using fetch::g_interrupted;

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
    options.pwm_dither_bits = 1;
    options.limit_refresh_rate_hz = 120;

    rgb_matrix::RuntimeOptions runtime;
    runtime.gpio_slowdown = 1;

    return unique_ptr<rgb_matrix::RGBMatrix>(
        rgb_matrix::RGBMatrix::CreateFromOptions(options, runtime)
    );
}

enum class Mode { None, Art, Weather };

}  // namespace

int main() {
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

    thread weather_thread(fetch::weather_loop);

    auto *canvas = matrix->CreateFrameCanvas();
    string prev_art_url;
    bool prev_paused = false;
    img::Bitmap art_buf;  // last decoded album-art bitmap, re-blittable on pause toggle
    double last_playing = 0.0;
    double last_weather_draw = 0.0;
    Mode mode = Mode::None;
    int current_brightness = -1;

    constexpr double PAUSED_DIM = 0.5;
    constexpr int PAUSE_PADDING = 3;
    constexpr int WIPE_STEP = 2;  // columns per frame; 64/2 = 32 frames

    const auto blit_art = [&](bool paused) {
        canvas->Clear();
        img::draw(art_buf, canvas, 0, 0, paused ? PAUSED_DIM : 1.0);
        if (paused) {
            draw::pause_icon(canvas, 63 - PAUSE_PADDING, 63 - PAUSE_PADDING);
        }
        canvas = matrix->SwapOnVSync(canvas);
    };

    // Horizontal column wipe from the currently displayed `art_buf` to `next`.
    // After the wipe completes, the caller should move `next` into `art_buf`.
    const auto wipe_art = [&](const img::Bitmap &next) {
        for (int split = WIPE_STEP; split < 64; split += WIPE_STEP) {
            if (g_interrupted.load()) return;
            canvas->Clear();
            img::draw_split(art_buf, next, canvas, 0, 0, split);
            canvas = matrix->SwapOnVSync(canvas);
        }
    };

    while (!g_interrupted.load()) {
        const int target_b = tu::is_night() ? cfg::NIGHT_BRIGHTNESS : cfg::DAY_BRIGHTNESS;
        if (target_b != current_brightness) {
            matrix->SetBrightness(target_b);
            current_brightness = target_b;
            log("brightness -> ", target_b);
            prev_art_url.clear();
            mode = Mode::None;
        }

        spotify::NowPlaying np;
        string err;
        const bool ok = spotify::current_playback(&np, &err);
        if (!ok) {
            log("spotify poll failed: ", err);
        }

        const double now = static_cast<double>(tu::now_unix());

        if (np.active && !np.album_art_url.empty()) {
            last_playing = now;
            const bool url_changed = np.album_art_url != prev_art_url;
            const bool pause_changed = np.paused != prev_paused;
            const bool first_art = art_buf.rgb.empty() || mode != Mode::Art;
            const bool need_fetch = url_changed || first_art;
            if (need_fetch) {
                string body, herr;
                if (!http::get(
                        np.album_art_url,
                        cfg::CONNECT_TIMEOUT_S,
                        cfg::READ_TIMEOUT_S,
                        &body,
                        &herr
                    )) {
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
        } else if (ok) {
            const double idle = now - last_playing;
            if (idle >= cfg::IDLE_TIMEOUT.count()) {
                const bool need_redraw =
                    mode != Mode::Weather ||
                    (now - last_weather_draw) >= cfg::WEATHER_REDRAW.count();
                if (need_redraw) {
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

    weather_thread.join();
    matrix->Clear();
    matrix.reset();
    http::global_cleanup();
    return 0;
}
