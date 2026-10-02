#pragma once

#include <chrono>
#include <cstdint>

#include "core/palette.h"

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
inline constexpr Color divider = palette::text_alpha20;

// Control center
inline constexpr int control_center_width = 320;
inline constexpr int control_center_padding = 16;
inline constexpr int control_center_row_height = 40;
inline constexpr int control_center_track_height = 6;
inline constexpr int control_center_icon_slot = 28;
inline constexpr int control_center_gap = 12;
inline constexpr int control_center_wheel_step = 5;
inline constexpr const char *control_center_percent_sample = "100%";
inline constexpr Color control_center_track = palette::text_alpha20;
inline constexpr Color control_center_fill = palette::accent;

// Typography
inline constexpr const char *font = "Comic Shanns Mono 15";
inline constexpr const char *icon_font = "tabler-icons 17";

// Colors
inline constexpr Color background = {palette::base.r, palette::base.g, palette::base.b, 0.8f};
inline constexpr Color border = palette::accent;
inline constexpr Color foreground = palette::text;
inline constexpr Color pill_active = palette::accent_alt;
inline constexpr Color pill_occupied = palette::accent;
inline constexpr Color pill_inactive = palette::text_alpha20;

// Content
inline constexpr const char *clock_format = "%a %Y-%m-%d %H:%M:%S";
inline constexpr const char *logout_label = "Logout";
inline constexpr const char *bluetooth_idle_label = "Idle";
inline constexpr const char *bluetooth_disabled_label = "Disabled";

// Memory
inline constexpr std::chrono::milliseconds trim_interval = std::chrono::minutes(1);

} // namespace astralia::bar_config
