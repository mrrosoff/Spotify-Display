#include "render/draw.hpp"

#include <cmath>
#include <cstdint>
#include <string>

using namespace std;

namespace draw {

int text_width(const Font &f, string_view s) {
    int w = 0;
    for (unsigned char c : s) w += f.CharacterWidth(static_cast<uint32_t>(c));
    return w;
}

void text_top(
    Canvas *c, const Font &f, int x, int top_y, const Color &color, string_view text
) {
    rgb_matrix::DrawText(c, f, x, top_y + f.baseline(), color, string(text).c_str());
}

void text_centered(
    Canvas *c, const Font &f, int cx, int cy, const Color &color, string_view text
) {
    const int w = text_width(f, text);
    const int top = cy - f.height() / 2;
    rgb_matrix::DrawText(
        c, f, cx - w / 2, top + f.baseline(), color, string(text).c_str()
    );
}

void pause_icon(Canvas *c, int x_right, int y_bottom) {
    // 7×9 icon: two 2-px-wide bars, 3-px gap, with a 1-px dark halo around
    // each bar so the icon reads on any background.
    constexpr int W = 7, H = 9;
    const int x0 = x_right - (W - 1);
    const int y0 = y_bottom - (H - 1);

    const Color halo{0, 0, 0};
    const Color fill{230, 230, 230};

    auto in_bar = [](int x, int y) {
        if (y < 0 || y >= H) return false;
        return (x >= 0 && x <= 1) || (x >= 5 && x <= 6);
    };

    for (int y = -1; y <= H; ++y) {
        for (int x = -1; x <= W; ++x) {
            const bool bar = in_bar(x, y);
            if (bar) {
                c->SetPixel(x0 + x, y0 + y, fill.r, fill.g, fill.b);
                continue;
            }
            const bool adj = in_bar(x - 1, y) || in_bar(x + 1, y) ||
                             in_bar(x, y - 1) || in_bar(x, y + 1);
            if (adj) c->SetPixel(x0 + x, y0 + y, halo.r, halo.g, halo.b);
        }
    }
}

void icon(Canvas *c, const XbmIcon &ic, int x0, int y0, const Color &color) {
    for (int y = 0; y < ic.height; ++y) {
        for (int x = 0; x < ic.width; ++x) {
            if (ic.pixels[y * ic.width + x]) {
                c->SetPixel(x0 + x, y0 + y, color.r, color.g, color.b);
            }
        }
    }
}

void spotify_logo(Canvas *c, int cx, int cy, const Color &color) {
    // Filled disc with three nested convex-up arc gaps. The arcs share a
    // center below the disc and stay inside the rim so the circle reads solid.
    constexpr int R = 13;
    constexpr double AC = 13.0;  // arc center, below the disc center
    constexpr double TH = 1.5;   // half-thickness of each gap
    const double rad[3] = {6.0, 11.0, 16.0};
    const double inner = (R - 2.0) * (R - 2.0);

    for (int y = -R; y <= R; ++y) {
        for (int x = -R; x <= R; ++x) {
            const double r2 = static_cast<double>(x * x + y * y);
            if (r2 > static_cast<double>(R * R)) continue;
            bool gap = false;
            if (r2 <= inner && y <= AC) {
                const double d = std::hypot(static_cast<double>(x), y - AC);
                for (const double r : rad) {
                    if (std::fabs(d - r) <= TH) {
                        gap = true;
                        break;
                    }
                }
            }
            if (!gap) c->SetPixel(cx + x, cy + y, color.r, color.g, color.b);
        }
    }
}

}  // namespace draw
