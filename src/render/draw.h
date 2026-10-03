#pragma once

#include <cairo.h>

#include "render/palette.h"

namespace astralia {

void set_source(cairo_t *cr, const Color &color);
void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r);

} // namespace astralia
