#pragma once

#include "graphics.h"
#include "led-matrix.h"
#include "util/xbm.hpp"

#include <string_view>

namespace draw {

using rgb_matrix::Canvas;
using rgb_matrix::Color;
using rgb_matrix::Font;

int text_width(const Font &f, std::string_view s);

void text_top(
    Canvas *c, const Font &f, int x, int top_y, const Color &color, std::string_view text
);

void text_centered(
    Canvas *c, const Font &f, int cx, int cy, const Color &color, std::string_view text
);

void icon(Canvas *c, const XbmIcon &ic, int x0, int y0, const Color &color);

void spotify_logo(Canvas *c, int cx, int cy, const Color &color);

// Pause icon (two vertical bars with a 1-px dark halo so it reads on any
// background). The icon's bottom-right corner is placed at (x_right, y_bottom).
void pause_icon(Canvas *c, int x_right, int y_bottom);

}  // namespace draw
