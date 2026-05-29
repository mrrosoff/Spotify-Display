#include "util/image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <algorithm>
#include <memory>

using namespace std;

namespace img {

bool decode(
    const string &buffer, int target_w, int target_h, Bitmap *out, string *error
) {
    int src_w = 0, src_h = 0, channels = 0;
    auto *raw = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc *>(buffer.data()),
        static_cast<int>(buffer.size()),
        &src_w,
        &src_h,
        &channels,
        3
    );
    if (!raw) {
        if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "decode failed";
        return false;
    }
    unique_ptr<stbi_uc, void (*)(void *)> pixels(raw, stbi_image_free);

    out->width = target_w;
    out->height = target_h;
    out->rgb.assign(static_cast<size_t>(target_w) * target_h * 3, 0);

    for (int dy = 0; dy < target_h; ++dy) {
        const int sy = (dy * src_h) / target_h;
        for (int dx = 0; dx < target_w; ++dx) {
            const int sx = (dx * src_w) / target_w;
            const stbi_uc *p = pixels.get() + (sy * src_w + sx) * 3;
            uint8_t *o = out->rgb.data() + (dy * target_w + dx) * 3;
            o[0] = p[0];
            o[1] = p[1];
            o[2] = p[2];
        }
    }
    return true;
}

void draw(const Bitmap &bm, rgb_matrix::Canvas *canvas, int x0, int y0) {
    for (int y = 0; y < bm.height; ++y) {
        for (int x = 0; x < bm.width; ++x) {
            const uint8_t *p = bm.rgb.data() + (y * bm.width + x) * 3;
            canvas->SetPixel(x0 + x, y0 + y, p[0], p[1], p[2]);
        }
    }
}

void draw_split(
    const Bitmap &left,
    const Bitmap &right,
    rgb_matrix::Canvas *canvas,
    int x0,
    int y0,
    int split
) {
    const int w = left.width;
    const int h = left.height;
    split = clamp(split, 0, w);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Bitmap &src = (x < split) ? right : left;
            const uint8_t *p = src.rgb.data() + (y * w + x) * 3;
            canvas->SetPixel(x0 + x, y0 + y, p[0], p[1], p[2]);
        }
    }
}

}  // namespace img
