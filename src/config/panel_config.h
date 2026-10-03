#pragma once

#include "render/palette.h"

namespace astralia::panel_config {

// Geometry
inline constexpr int width = 360;
inline constexpr int max_height = 560;
inline constexpr int padding = 14;
inline constexpr int header_height = 30;
inline constexpr int row_height = 46;
inline constexpr int section_height = 26;
inline constexpr int label_height = 24;
inline constexpr int slider_height = 32;
inline constexpr int empty_height = 60;
inline constexpr int row_gap = 6;
inline constexpr int card_gap = 8;
inline constexpr int button_size = 26;
inline constexpr int toggle_width = 36;
inline constexpr int toggle_height = 20;
inline constexpr int track_height = 6;
inline constexpr int scroll_step = 40;
inline constexpr float border_width = metrics::border_thin;

// Typography
inline constexpr const char *title_font = "Comic Shanns Mono 15";
inline constexpr const char *font = "Comic Shanns Mono 12";
inline constexpr const char *small_font = "Comic Shanns Mono 10";
inline constexpr const char *icon_font = "tabler-icons 15";

// Colors
inline constexpr Color background = {palette::base.r, palette::base.g, palette::base.b, 0.8f};
inline constexpr Color border = palette::accent;
inline constexpr Color foreground = palette::text;
inline constexpr Color track = palette::text_alpha20;
inline constexpr Color fill = palette::accent;
inline constexpr Color row = palette::text_alpha06;
inline constexpr Color row_active = palette::accent_alpha25;
inline constexpr Color row_busy = palette::accent_alpha12;
inline constexpr Color divider = palette::text_alpha08;
inline constexpr Color button = palette::text_alpha08;
inline constexpr Color error = palette::critical_alpha15;

} // namespace astralia::panel_config
