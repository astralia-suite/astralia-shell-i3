#pragma once

#include <optional>
#include <vector>

#include "config/bar_config.h"

namespace astralia {

struct BarRect {
    int x;
    int y;
    int width;
    int height;

    bool contains(int px) const { return px >= x && px < x + width; }
};

struct BarStyleSpec {
    int margin_x;
    int margin_top;
    int padding;
    double corner_radius;
    double rail_height;
    double island_radius;
    double fillet_radius;
};

struct IslandSpan {
    double left;
    double right;
};

struct IslandShape {
    double outer_x;
    double outer_width;
    double inner_x;
    double inner_width;
};

struct Fillet {
    double edge_x;
    bool right_of_island;
};

struct OkinamiFrame {
    std::vector<IslandShape> islands;
    std::vector<Fillet> fillets;
};

const BarStyleSpec &bar_style_spec(BarStyle style);
bool bar_style_has_rail(const BarStyleSpec &spec);
int bar_window_height(const BarStyleSpec &spec);
BarRect bar_panel_rect(const BarStyleSpec &spec, int width);
int bar_panel_top(const BarStyleSpec &spec);
IslandShape bar_island_shape(const BarStyleSpec &spec, double left, double right, bool flush_left, bool flush_right);
OkinamiFrame bar_okinami_frame(const BarStyleSpec &spec, std::optional<double> left_end, std::optional<IslandSpan> center, std::optional<double> right_start, double width);

} // namespace astralia
