#pragma once

#include "led-matrix.h"

#include <cstdint>
#include <string>
#include <vector>

namespace img {

struct Bitmap {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;  // width * height * 3, row-major
};

// Decode a JPEG/PNG buffer and nearest-neighbor-resample into a target_w ×
// target_h RGB bitmap. Returns false on decode error.
bool decode(
    const std::string &buffer,
    int target_w,
    int target_h,
    Bitmap *out,
    std::string *error
);

// Blit a bitmap to the canvas at (x0, y0) at full color. Dimming is handled
// by the matrix's CIE-mapped SetBrightness, not by scaling RGB here.
void draw(const Bitmap &bm, rgb_matrix::Canvas *canvas, int x0, int y0);

// Composite two same-size bitmaps onto the canvas: columns [0, split) come
// from `right` (the new image), columns [split, width) come from `left` (the
// old image). Used by the album-art crossover wipe.
void draw_split(
    const Bitmap &left,
    const Bitmap &right,
    rgb_matrix::Canvas *canvas,
    int x0,
    int y0,
    int split
);

}  // namespace img
