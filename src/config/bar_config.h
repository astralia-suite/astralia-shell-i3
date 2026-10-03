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

// Control center
inline constexpr int control_center_row_height = 40;
inline constexpr int control_center_icon_slot = 28;
inline constexpr int control_center_gap = 12;
inline constexpr int control_center_wheel_step = 5;
inline constexpr const char *control_center_percent_sample = "muted";

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

// Typography
inline constexpr const char *font = "Comic Shanns Mono 15";
inline constexpr const char *icon_font = "tabler-icons 17";

// Content
inline constexpr const char *clock_format = "%a %Y-%m-%d %H:%M:%S";
inline constexpr const char *logout_label = "Logout";
inline constexpr const char *bluetooth_idle_label = "Idle";
inline constexpr const char *bluetooth_disabled_label = "Disabled";

// Timing
inline constexpr std::chrono::milliseconds panel_close_linger{50};

// Memory
inline constexpr std::chrono::milliseconds trim_interval = std::chrono::minutes(1);

} // namespace astralia::bar_config
