#include "render/draw.hpp"

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

void icon(Canvas *c, const XbmIcon &ic, int x0, int y0, const Color &color) {
    for (int y = 0; y < ic.height; ++y) {
        for (int x = 0; x < ic.width; ++x) {
            if (ic.pixels[y * ic.width + x]) {
                c->SetPixel(x0 + x, y0 + y, color.r, color.g, color.b);
            }
        }
    }
}

}  // namespace draw
