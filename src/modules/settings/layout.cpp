#include <algorithm>
#include <cmath>

#include "config/settings_config.h"

#include "modules/settings/layout.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

constexpr double cell_pitch = cfg::thumb_size + cfg::thumb_gap;
constexpr auto columns = static_cast<std::size_t>(cfg::thumb_columns);

std::size_t row_count(std::size_t count) {
    return (count + columns - 1) / columns;
}

} // namespace

std::pair<int, int> settings_window_size(int output_width, int output_height) {
    return {std::min(cfg::card_max_width, output_width - 2 * cfg::card_margin),
            std::min(cfg::card_max_height, output_height - 2 * cfg::card_margin)};
}

SettingsGeometry settings_geometry(int width, int height) {
    SettingsGeometry geometry;
    geometry.card = {0, 0, static_cast<double>(width), static_cast<double>(height)};
    geometry.rail = {0, 0, cfg::rail_width, static_cast<double>(height)};
    geometry.content = {cfg::rail_width + cfg::content_padding, cfg::content_padding,
                        width - cfg::rail_width - 2 * cfg::content_padding,
                        height - 2 * cfg::content_padding};
    return geometry;
}

PanelRect settings_tab_rect(const SettingsGeometry &geometry, std::size_t index) {
    double top = geometry.rail.y + cfg::rail_padding + cfg::rail_title_height;
    return {geometry.rail.x + cfg::rail_padding,
            top + static_cast<double>(index) * (cfg::tab_height + cfg::tab_gap),
            geometry.rail.w - 2 * cfg::rail_padding, cfg::tab_height};
}

std::optional<std::size_t> settings_tab_at(const SettingsGeometry &geometry, double x, double y) {
    for (std::size_t i = 0; i < cfg::tab_count; ++i) {
        if (settings_tab_rect(geometry, i).contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<PanelRect> settings_chip_rects(std::size_t count, const PanelRect &area) {
    std::vector<PanelRect> rects;
    if (count == 0) {
        return rects;
    }
    double width = (area.w - static_cast<double>(count - 1) * cfg::chip_gap) / static_cast<double>(count);
    for (std::size_t i = 0; i < count; ++i) {
        rects.push_back({area.x + static_cast<double>(i) * (width + cfg::chip_gap), area.y, width, cfg::chip_height});
    }
    return rects;
}

double settings_grid_width() { return static_cast<double>(columns) * cfg::thumb_size + static_cast<double>(columns - 1) * cfg::thumb_gap; }

double settings_grid_content_height(std::size_t count) {
    std::size_t rows = row_count(count);
    return rows == 0 ? 0.0 : static_cast<double>(rows) * cell_pitch - cfg::thumb_gap;
}

double settings_clamp_scroll(double scroll, std::size_t count, double grid_height) {
    return std::clamp(scroll, 0.0, std::max(0.0, settings_grid_content_height(count) - grid_height));
}

PanelRect settings_grid_cell(const PanelRect &grid, std::size_t index, double scroll) {
    std::size_t column = index % columns;
    std::size_t row = index / columns;
    return {grid.x + static_cast<double>(column) * cell_pitch,
            grid.y + static_cast<double>(row) * cell_pitch - scroll, cfg::thumb_size, cfg::thumb_size};
}

std::pair<std::size_t, std::size_t> settings_grid_visible(const PanelRect &grid, std::size_t count, double scroll) {
    if (count == 0) {
        return {0, 0};
    }
    auto first_row = static_cast<std::size_t>(std::max(0.0, std::floor(scroll / cell_pitch)));
    auto last_row = static_cast<std::size_t>(std::max(0.0, std::floor((scroll + grid.h) / cell_pitch)));
    std::size_t first = std::min(first_row * columns, count);
    std::size_t last = std::min((last_row + 1) * columns, count);
    return {first, last};
}

std::optional<std::size_t> settings_grid_cell_at(const PanelRect &grid, std::size_t count, double scroll, double x, double y) {
    if (!grid.contains(x, y)) {
        return std::nullopt;
    }
    auto [first, last] = settings_grid_visible(grid, count, scroll);
    for (std::size_t i = first; i < last; ++i) {
        if (settings_grid_cell(grid, i, scroll).contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace astralia
