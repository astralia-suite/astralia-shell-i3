#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "render/panel_chrome.h"

namespace astralia {

struct SettingsGeometry {
    PanelRect card;
    PanelRect header;
    PanelRect close;
    PanelRect profile;
    PanelRect rail;
    PanelRect content;
    double divider_x = 0;
    double header_divider_y = 0;
    bool expanded = true;
};

std::pair<int, int> settings_window_size(int output_width, int output_height);
SettingsGeometry settings_geometry(int width, int height);
PanelRect settings_tab_rect(const SettingsGeometry &geometry, std::size_t index);
std::optional<std::size_t> settings_tab_at(const SettingsGeometry &geometry, double x, double y);
std::vector<PanelRect> settings_chip_rects(std::size_t count, const PanelRect &area);
double settings_grid_width();
double settings_grid_content_height(std::size_t count);
double settings_clamp_scroll(double scroll, std::size_t count, double grid_height);
PanelRect settings_grid_cell(const PanelRect &grid, std::size_t index, double scroll);
std::pair<std::size_t, std::size_t> settings_grid_visible(const PanelRect &grid, std::size_t count, double scroll);
std::optional<std::size_t> settings_grid_cell_at(const PanelRect &grid, std::size_t count, double scroll, double x, double y);

} // namespace astralia
