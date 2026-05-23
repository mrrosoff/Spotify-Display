#include "util/image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <memory>

namespace img {

bool decode_and_draw(
    const std::string &buffer,
    rgb_matrix::Canvas *canvas,
    int x0,
    int y0,
    int w,
    int h,
    std::string *error
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
    std::unique_ptr<stbi_uc, void (*)(void *)> pixels(raw, stbi_image_free);

    for (int dy = 0; dy < h; ++dy) {
        const int sy = (dy * src_h) / h;
        for (int dx = 0; dx < w; ++dx) {
            const int sx = (dx * src_w) / w;
            const stbi_uc *p = pixels.get() + (sy * src_w + sx) * 3;
            canvas->SetPixel(x0 + dx, y0 + dy, p[0], p[1], p[2]);
        }
    }
    return true;
}

}  // namespace img
