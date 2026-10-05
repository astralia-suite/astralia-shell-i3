#pragma once

#include <cairo.h>

#include "modules/bar/styles/geometry.h"

namespace astralia {

const BarStyleSpec &continuous_style_spec();
void paint_continuous(cairo_t *cr, const BarStyleSpec &spec, const BarRect &rect);

} // namespace astralia
