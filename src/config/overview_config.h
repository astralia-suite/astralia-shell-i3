#pragma once

namespace astralia::overview_config {

// Grid
inline constexpr int columns = 5;
inline constexpr int rows = 2;
inline constexpr int per_page = columns * rows;
inline constexpr double scale = 0.15;
inline constexpr double spacing = 5.0;
inline constexpr double padding = 10.0;
inline constexpr double screen_margin = 20.0;

// Rounding and borders
inline constexpr double screen_rounding = 23.0;
inline constexpr double window_rounding = 18.0;
inline constexpr double background_border_width = 2.0;
inline constexpr double workspace_border_width = 2.0;
inline constexpr double window_border_width = 2.0;
inline constexpr double indicator_border_width = 2.0;

// Workspace label
inline constexpr const char *number_font = "Comic Shanns Mono 40px";
inline constexpr double number_fade = 0.8;

// Window tile
inline constexpr double icon_to_tile_ratio = 0.5;
inline constexpr int icon_min_size = 16;
inline constexpr int icon_max_size = 48;
inline constexpr int icon_size_step = 4;

// Interaction
inline constexpr int focus_grace_ms = 300;

} // namespace astralia::overview_config
