#pragma once

#include <cairo.h>

#include "modules/bar/styles/geometry.h"

namespace astralia {

const BarStyleSpec &okinami_style_spec();
void paint_okinami(cairo_t *cr, const BarStyleSpec &spec, int width, int height, const OkinamiFrame &frame);

} // namespace astralia
