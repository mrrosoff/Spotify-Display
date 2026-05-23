#pragma once

#include "led-matrix.h"

#include <string>

namespace img {

// Decode a JPEG/PNG/etc. buffer and draw it to the canvas, scaling to
// `w` × `h` with nearest-neighbor and origin at (x0, y0). Returns false
// on decode error and fills `error`.
bool decode_and_draw(
    const std::string &buffer,
    rgb_matrix::Canvas *canvas,
    int x0,
    int y0,
    int w,
    int h,
    std::string *error
);

}  // namespace img
