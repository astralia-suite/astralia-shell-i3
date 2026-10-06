#include <algorithm>
#include <cmath>

#include "config/overview_config.h"

#include "modules/overview/layout.h"

namespace astralia {

namespace cfg = overview_config;

uint32_t overview_workspace_at(uint32_t page, int row, int col) {
    return page * cfg::per_page + static_cast<uint32_t>(row * cfg::columns + col) + 1;
}

uint32_t overview_page_of(uint32_t workspace) {
    return workspace > 0 ? (workspace - 1) / cfg::per_page : 0;
}

OverviewLayout overview_layout(double surface_width, double surface_height, double work_width, double work_height, uint32_t page) {
    OverviewLayout layout;
    work_width = std::max(1.0, work_width);
    work_height = std::max(1.0, work_height);
    double frame = 2.0 * (cfg::padding + cfg::screen_margin);
    double gap_x = cfg::spacing * (cfg::columns - 1);
    double gap_y = cfg::spacing * (cfg::rows - 1);
    double fit_x = (surface_width - frame - gap_x) / (work_width * cfg::columns);
    double fit_y = (surface_height - frame - gap_y) / (work_height * cfg::rows);
    layout.scale = std::max(0.01, std::min({cfg::scale, fit_x, fit_y}));

    double cell_w = std::round(work_width * layout.scale);
    double cell_h = std::round(work_height * layout.scale);
    double panel_w = cell_w * cfg::columns + gap_x + 2.0 * cfg::padding;
    double panel_h = cell_h * cfg::rows + gap_y + 2.0 * cfg::padding;
    layout.panel = {std::round((surface_width - panel_w) / 2.0), std::round((surface_height - panel_h) / 2.0), panel_w, panel_h};

    for (int row = 0; row < cfg::rows; ++row) {
        for (int col = 0; col < cfg::columns; ++col) {
            layout.cells.push_back({overview_workspace_at(page, row, col),
                                    {layout.panel.x + cfg::padding + col * (cell_w + cfg::spacing),
                                     layout.panel.y + cfg::padding + row * (cell_h + cfg::spacing), cell_w, cell_h}});
        }
    }
    return layout;
}

const OverviewCell *overview_find_cell(const OverviewLayout &layout, uint32_t workspace) {
    for (const OverviewCell &cell : layout.cells) {
        if (cell.workspace == workspace) {
            return &cell;
        }
    }
    return nullptr;
}

const OverviewCell *overview_cell_at(const OverviewLayout &layout, double x, double y) {
    for (const OverviewCell &cell : layout.cells) {
        if (cell.rect.contains(x, y)) {
            return &cell;
        }
    }
    return nullptr;
}

OverviewRect overview_tile_rect(const OverviewRect &cell, double work_width, double work_height, double x, double y, double width, double height) {
    work_width = std::max(1.0, work_width);
    work_height = std::max(1.0, work_height);
    double scale = std::min(cell.width / work_width, cell.height / work_height);
    double w = std::min(std::max(1.0, width * scale), cell.width);
    double h = std::min(std::max(1.0, height * scale), cell.height);
    double left = std::clamp(std::max(x * scale, 0.0), 0.0, std::max(0.0, cell.width - w));
    double top = std::clamp(std::max(y * scale, 0.0), 0.0, std::max(0.0, cell.height - h));
    return {cell.x + left, cell.y + top, w, h};
}

uint32_t overview_step(uint32_t workspace, int d_col, int d_row) {
    uint32_t page = overview_page_of(workspace);
    int index = static_cast<int>((workspace - 1) % cfg::per_page);
    int col = (index % cfg::columns + d_col + cfg::columns) % cfg::columns;
    int row = (index / cfg::columns + d_row + cfg::rows) % cfg::rows;
    return overview_workspace_at(page, row, col);
}

} // namespace astralia
