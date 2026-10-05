#include <cmath>

#include "modules/bar/styles/continuous.h"
#include "modules/bar/styles/geometry.h"
#include "modules/bar/styles/okinami.h"

namespace astralia {

const BarStyleSpec &bar_style_spec(BarStyle style) {
    return style == BarStyle::okinami ? okinami_style_spec() : continuous_style_spec();
}

bool bar_style_has_rail(const BarStyleSpec &spec) { return spec.rail_height > 0.0; }

int bar_window_height(const BarStyleSpec &spec) { return spec.margin_top + bar_config::height; }

BarRect bar_panel_rect(const BarStyleSpec &spec, int width) {
    return {spec.margin_x, spec.margin_top, width - 2 * spec.margin_x, bar_config::height};
}

int bar_panel_top(const BarStyleSpec &spec) { return bar_window_height(spec) + bar_config::panel_gap; }

IslandShape bar_island_shape(const BarStyleSpec &spec, double left, double right, bool flush_left, bool flush_right) {
    constexpr double border = bar_config::border_width;
    IslandShape shape{};
    shape.outer_x = flush_left ? left - spec.island_radius : left;
    shape.outer_width = (flush_right ? right + spec.island_radius : right) - shape.outer_x;
    shape.inner_x = flush_left ? shape.outer_x : shape.outer_x + border;
    shape.inner_width = shape.outer_x + shape.outer_width - (flush_right ? 0.0 : border) - shape.inner_x;
    return shape;
}

OkinamiFrame bar_okinami_frame(const BarStyleSpec &spec, std::optional<double> left_end, std::optional<IslandSpan> center, std::optional<double> right_start, double width) {
    OkinamiFrame frame;
    if (left_end) {
        frame.islands.push_back(bar_island_shape(spec, 0.0, *left_end, true, false));
        frame.fillets.push_back({*left_end, true});
    }
    if (center) {
        frame.islands.push_back(bar_island_shape(spec, center->left, center->right, false, false));
        frame.fillets.push_back({center->left, false});
        frame.fillets.push_back({center->right, true});
    }
    if (right_start) {
        frame.islands.push_back(bar_island_shape(spec, *right_start, width, false, true));
        frame.fillets.push_back({*right_start, false});
    }
    return frame;
}

} // namespace astralia
