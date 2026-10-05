#include "config/bar_config.h"

#include "modules/bar/styles/continuous.h"

#include "render/draw.h"

namespace astralia {

namespace {

constexpr BarStyleSpec style{bar_config::margin_x, bar_config::margin_top, bar_config::pill_pad, bar_config::height / 2.0, 0.0, 0.0, 0.0};

} // namespace

const BarStyleSpec &continuous_style_spec() { return style; }

void paint_continuous(cairo_t *cr, const BarStyleSpec &spec, const BarRect &rect) {
    constexpr double inset = bar_config::border_width / 2.0;
    set_source(cr, palette::base_alpha80);
    rounded_rect(cr, rect.x, rect.y, rect.width, rect.height, spec.corner_radius);
    cairo_fill(cr);
    set_source(cr, palette::accent);
    cairo_set_line_width(cr, bar_config::border_width);
    rounded_rect(cr, rect.x + inset, rect.y + inset, rect.width - 2 * inset, rect.height - 2 * inset, spec.corner_radius - inset);
    cairo_stroke(cr);
}

} // namespace astralia
