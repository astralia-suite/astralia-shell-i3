#include <numbers>

#include "config/bar_config.h"

#include "modules/bar/styles/okinami.h"

#include "render/draw.h"

namespace astralia {

namespace {

constexpr BarStyleSpec style{0, 0, bar_config::island_padding, 0.0, bar_config::rail_height, bar_config::island_radius, bar_config::fillet_radius};

constexpr double border = bar_config::border_width;

void island_path(cairo_t *cr, double x, double width, double height, double radius) {
    rounded_rect(cr, x, -radius, width, height + radius, radius);
}

void fillet_path(cairo_t *cr, double x, double y, double size, double center_x, double center_y, double radius) {
    cairo_rectangle(cr, x, y, size, size);
    cairo_clip(cr);
    cairo_new_path(cr);
    cairo_rectangle(cr, x, y, size, size);
    cairo_arc(cr, center_x, center_y, radius, 0.0, 2.0 * std::numbers::pi);
}

void fillet_outer(cairo_t *cr, const BarStyleSpec &spec, const Fillet &fillet) {
    double x = fillet.right_of_island ? fillet.edge_x : fillet.edge_x - spec.fillet_radius;
    double center_x = fillet.right_of_island ? fillet.edge_x + spec.fillet_radius : fillet.edge_x - spec.fillet_radius;
    cairo_save(cr);
    fillet_path(cr, x, spec.rail_height, spec.fillet_radius, center_x, spec.rail_height + spec.fillet_radius, spec.fillet_radius);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill(cr);
    cairo_restore(cr);
}

void fillet_inner(cairo_t *cr, const BarStyleSpec &spec, const Fillet &fillet) {
    double size = spec.fillet_radius + border;
    double x = fillet.right_of_island ? fillet.edge_x - border : fillet.edge_x - spec.fillet_radius;
    double center_x = fillet.right_of_island ? fillet.edge_x + spec.fillet_radius : fillet.edge_x - spec.fillet_radius;
    cairo_save(cr);
    fillet_path(cr, x, spec.rail_height - border, size, center_x, spec.rail_height + spec.fillet_radius, size);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill(cr);
    cairo_restore(cr);
}

} // namespace

const BarStyleSpec &okinami_style_spec() { return style; }

void paint_okinami(cairo_t *cr, const BarStyleSpec &spec, int width, int height, const OkinamiFrame &frame) {
    auto w = static_cast<double>(width);
    auto h = static_cast<double>(height);
    set_source(cr, palette::accent);
    cairo_rectangle(cr, 0.0, 0.0, w, spec.rail_height);
    cairo_fill(cr);
    for (const IslandShape &island : frame.islands) {
        island_path(cr, island.outer_x, island.outer_width, h, spec.island_radius);
        cairo_fill(cr);
    }
    for (const Fillet &fillet : frame.fillets) {
        fillet_outer(cr, spec, fillet);
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    set_source(cr, palette::base_alpha80);
    cairo_rectangle(cr, 0.0, 0.0, w, spec.rail_height - border);
    cairo_fill(cr);
    for (const IslandShape &island : frame.islands) {
        rounded_rect(cr, island.inner_x, -spec.island_radius, island.inner_width, h + spec.island_radius - border, spec.island_radius - border);
        cairo_fill(cr);
    }
    for (const Fillet &fillet : frame.fillets) {
        fillet_inner(cr, spec, fillet);
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
}

} // namespace astralia
