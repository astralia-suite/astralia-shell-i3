#pragma once

#include <cstdint>
#include <vector>

namespace astralia {

struct OverviewRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    bool contains(double px, double py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
    bool operator==(const OverviewRect &) const = default;
};

struct OverviewCell {
    uint32_t workspace = 0;
    OverviewRect rect;
};

struct OverviewLayout {
    OverviewRect panel;
    double scale = 0.0;
    std::vector<OverviewCell> cells;
};

uint32_t overview_workspace_at(uint32_t page, int row, int col);
uint32_t overview_page_of(uint32_t workspace);
OverviewLayout overview_layout(double surface_width, double surface_height, double work_width, double work_height, uint32_t page);
const OverviewCell *overview_find_cell(const OverviewLayout &layout, uint32_t workspace);
const OverviewCell *overview_cell_at(const OverviewLayout &layout, double x, double y);
OverviewRect overview_tile_rect(const OverviewRect &cell, double work_width, double work_height, double x, double y, double width, double height);
uint32_t overview_step(uint32_t workspace, int d_col, int d_row);

} // namespace astralia
