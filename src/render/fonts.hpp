#pragma once

#include "graphics.h"

struct Fonts {
    rgb_matrix::Font title;  // 6x10
    rgb_matrix::Font row;    // 5x7
    rgb_matrix::Font dir;    // 4x6 (small caption — e.g. corner clock)

    bool load();
};
