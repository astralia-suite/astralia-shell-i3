#pragma once

#include <cairo.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/x_connection.h"

#include "render/panel_chrome.h"

namespace astralia {

void settings_draw_chips(cairo_t *cr, const PanelRect &area, const std::optional<std::string> &default_key, const std::vector<Output> &outputs, const std::string &selected, int action, std::vector<PanelHit> &hits);
void settings_draw_toggle_tile(cairo_t *cr, const PanelRect &rect, std::string_view label, bool on, int action, std::string tag, std::vector<PanelHit> &hits);

} // namespace astralia
