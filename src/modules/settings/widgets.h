#pragma once

#include <cairo.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/x_connection.h"

#include "modules/settings/layout.h"

#include "render/panel_chrome.h"

namespace astralia {

void settings_draw_chips(cairo_t *cr, const PanelRect &area, const std::optional<std::string> &default_key, const std::vector<Output> &outputs, const std::string &selected, int action, std::vector<PanelHit> &hits);
void settings_draw_choice(cairo_t *cr, const PanelRect &rect, std::string_view label, bool active, int action, std::string tag, std::vector<PanelHit> &hits);
void settings_draw_card(cairo_t *cr, const PanelRect &rect);
void settings_draw_profile(cairo_t *cr, const SettingsGeometry &geometry, std::string_view name, std::string_view uptime);
void settings_draw_rail(cairo_t *cr, const SettingsGeometry &geometry, std::size_t selected);
void settings_draw_toggle_tile(cairo_t *cr, const PanelRect &rect, std::string_view label, bool on, int action, std::string tag, std::vector<PanelHit> &hits);

} // namespace astralia
