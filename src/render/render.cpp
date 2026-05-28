#include "render/render.hpp"

#include "core/colors.hpp"
#include "data/caches.hpp"
#include "data/weather_codes.hpp"
#include "render/draw.hpp"
#include "util/time_utils.hpp"

#include "graphics.h"

#include <cstdio>
#include <mutex>
#include <string>

using namespace std;

namespace render {

using rgb_matrix::Canvas;

void loading(Canvas *canvas, const Fonts &fonts) {
    canvas->Clear();
    draw::text_centered(canvas, fonts.row, 32, 32, colors::LABEL, "Loading...");
}

void weather(Canvas *canvas, const Fonts &fonts, const map<string, XbmIcon> &icons) {
    canvas->Clear();

    WeatherData data;
    bool have;
    int day_index;
    int fails;
    {
        lock_guard lg(caches::mtx);
        have = caches::weather.have;
        data = caches::weather.data;
        day_index = caches::weather.day_index < 0 ? 0 : caches::weather.day_index;
        fails = caches::weather.consecutive_failures;
    }

    const auto date = tu::month_day_for(0);
    draw::text_top(canvas, fonts.title, 2, 1, colors::GREY, date);
    const auto clock = tu::clock_hhmm();
    const int cw = draw::text_width(fonts.dir, clock);
    draw::text_top(canvas, fonts.dir, 63 - cw, 4, colors::LABEL, clock);
    rgb_matrix::DrawLine(canvas, 0, 11, 63, 11, colors::DIM);

    if (!have) {
        const bool errored = fails >= 5;
        draw::text_centered(
            canvas,
            fonts.row,
            32,
            38,
            errored ? colors::RED : colors::LABEL,
            errored ? "Error" : "Loading..."
        );
        return;
    }

    const auto icon_name = string(icon_for_code(data.code));
    auto px_it = icons.find(icon_name);
    if (px_it == icons.end()) px_it = icons.find("cloud");
    if (px_it != icons.end()) {
        draw::icon(canvas, px_it->second, 6, 17, colors::ICON);
    }
    auto word = string(word_for_code(data.code));
    tu::to_upper(word);
    if (day_index == 1) {
        constexpr int PLUS_W = 3;
        constexpr int PLUS_GAP = 2;
        const int ww = draw::text_width(fonts.row, word);
        const int x0 = 32 - (ww + PLUS_GAP + PLUS_W) / 2;
        draw::text_top(canvas, fonts.row, x0, 55, colors::LABEL, word);
        const int px = x0 + ww + PLUS_GAP;
        const int py = 58;
        const auto &c = colors::LABEL;
        canvas->SetPixel(px + 1, py + 0, c.r, c.g, c.b);
        canvas->SetPixel(px + 0, py + 1, c.r, c.g, c.b);
        canvas->SetPixel(px + 1, py + 1, c.r, c.g, c.b);
        canvas->SetPixel(px + 2, py + 1, c.r, c.g, c.b);
        canvas->SetPixel(px + 1, py + 2, c.r, c.g, c.b);
    } else {
        draw::text_centered(canvas, fonts.row, 32, 55, colors::LABEL, word);
    }

    constexpr int rx = 41;
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d\xc2\xb0", data.hi);
    draw::text_top(canvas, fonts.title, rx, 17, colors::YELLOW, tmp);
    snprintf(tmp, sizeof(tmp), "%d\xc2\xb0", data.lo);
    draw::text_top(canvas, fonts.row, rx, 29, colors::LABEL, tmp);
    snprintf(tmp, sizeof(tmp), "%d%%", data.precip);
    draw::text_top(canvas, fonts.row, rx, 39, colors::PRECIP_BLUE, tmp);
}

}  // namespace render
