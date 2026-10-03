#pragma once

#include <cairo.h>

#include "render/panel_chrome.h"

namespace astralia {

int slider_percent_at(int track_x, int track_width, int px);
void draw_slider(cairo_t *cr, const PanelRect &track, int percent, bool muted, bool focused = false);

} // namespace astralia
