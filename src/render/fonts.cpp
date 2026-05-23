#include "render/fonts.hpp"

bool Fonts::load() {
    return title.LoadFont("fonts/6x10.bdf") && row.LoadFont("fonts/5x7.bdf") &&
           dir.LoadFont("fonts/4x6.bdf");
}
