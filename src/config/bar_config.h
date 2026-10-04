#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "render/palette.h"

namespace astralia::bar_config {

// Geometry
inline constexpr uint16_t height = 40;
inline constexpr uint16_t margin_x = 20;
inline constexpr uint16_t margin_top = 10;
inline constexpr double corner_radius = height / 2.0;
inline constexpr int padding_x = height / 2;
inline constexpr int group_gap = 16;
inline constexpr int item_gap = 14;
inline constexpr int label_gap = 6;
inline constexpr float border_width = metrics::border_thin;

// Workspace pills
inline constexpr uint32_t workspace_count = 10;
inline constexpr int pill_height = 10;
inline constexpr int pill_width = 10;
inline constexpr int pill_active_width = 26;
inline constexpr int pill_spacing = 6;

// Dividers
inline constexpr double divider_height_ratio = 0.4;
inline constexpr double divider_width = 1.0;

// Brightness panel
inline constexpr int brightness_row_height = 40;
inline constexpr int brightness_icon_slot = 28;
inline constexpr int brightness_gap = 12;
inline constexpr int brightness_wheel_step = 5;
inline constexpr const char *brightness_percent_sample = "100%";

// Panels
inline constexpr int panel_width = 360;
inline constexpr int panel_max_height = 560;
inline constexpr int panel_row_height = 46;
inline constexpr int panel_section_height = 26;
inline constexpr int panel_slider_height = 32;
inline constexpr int panel_empty_height = 60;
inline constexpr int panel_card_gap = 8;
inline constexpr int panel_scroll_step = 40;
inline constexpr int panel_top = 2 * margin_top + height;
inline constexpr int panel_percent_width = 56;
inline constexpr int panel_volume_step = 5;
inline constexpr std::size_t panel_password_min = 8;
inline constexpr const char *panel_echo_file = "electro.png";
inline constexpr int panel_echo_size = 20;
inline constexpr int panel_echo_row_height = 40;

// Media panel
inline constexpr int media_thumb_size = 72;
inline constexpr double media_thumb_radius = 8.0;
inline constexpr int media_title_gap = 12;
inline constexpr int media_title_spacing = 3;
inline constexpr int media_progress_top = 10;
inline constexpr int media_progress_height = 20;
inline constexpr int media_controls_top = 8;
inline constexpr int media_controls_height = 32;
inline constexpr int media_controls_spacing = 8;
inline constexpr int media_side_button = 28;
inline constexpr int media_play_button = 32;
inline constexpr std::chrono::milliseconds media_poll_interval{1000};

// Clock panel
inline constexpr int clock_panel_width = 504;
inline constexpr int clock_column_gap = 16;
inline constexpr int clock_weekday_line = 26;
inline constexpr int clock_date_line = 18;
inline constexpr int clock_line_gap = 2;
inline constexpr int clock_big_day_row = 74;
inline constexpr int clock_big_day_gap = 6;
inline constexpr int clock_week_line = 16;
inline constexpr int clock_grid_header = 24;
inline constexpr int clock_grid_header_gap = 15;
inline constexpr int clock_weekday_row = 22;
inline constexpr int clock_grid_top_gap = 2;
inline constexpr int clock_cell_padding = 4;
inline constexpr int clock_nav_button = 20;
inline constexpr int clock_nav_gap = 6;
inline constexpr int clock_today_dot = 6;
inline constexpr const char *clock_big_day_font = "Comic Shanns Mono 48";

// Tray panel and menu
inline constexpr int tray_cell_size = 48;
inline constexpr int tray_icon_size = 24;
inline constexpr int tray_grid_gap = 8;
inline constexpr int tray_menu_width = 240;
inline constexpr int tray_menu_padding = 6;
inline constexpr int tray_menu_row_height = 32;
inline constexpr int tray_menu_separator_height = 9;
inline constexpr int tray_menu_row_padding = 10;
inline constexpr int tray_menu_offset = 4;

// Typography
inline constexpr const char *font = "Comic Shanns Mono 15";
inline constexpr const char *icon_font = "tabler-icons 17";

// Content
inline constexpr const char *clock_format = "%a %Y-%m-%d %H:%M:%S";
inline constexpr const char *logout_label = "Logout";
inline constexpr const char *media_label = "Media";
inline constexpr const char *tray_label = "Tray";
inline constexpr const char *bluetooth_idle_label = "Idle";
inline constexpr const char *bluetooth_disabled_label = "Disabled";

// Timing
inline constexpr std::chrono::milliseconds panel_close_linger{50};

// Memory
inline constexpr std::chrono::milliseconds trim_interval = std::chrono::minutes(1);

} // namespace astralia::bar_config
