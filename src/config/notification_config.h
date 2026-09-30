#pragma once

#include <chrono>

#include "core/palette.h"

namespace astralia::notification_config {

// Placement
inline constexpr int margin_right = 10;
inline constexpr int margin_bottom = 10;
inline constexpr int spacing = 8;
inline constexpr double max_stack_height = 480.0;

// Card geometry
inline constexpr int card_width = 400;
inline constexpr double card_pad = 16.0;
inline constexpr double card_radius = metrics::radius_md;
inline constexpr double border_width = metrics::border_thin;
inline constexpr double content_spacing = 10.0;
inline constexpr double extra_height = 8.0;
inline constexpr int wrap_width = 320;

// Typography
inline constexpr const char *app_font = "Comic Shanns Mono Bold 13";
inline constexpr const char *summary_font = "Comic Shanns Mono Semi-Bold 17";
inline constexpr const char *body_font = "Comic Shanns Mono 15";

// Colors
inline constexpr Color background = palette::overlay;
inline constexpr Color border = palette::accent;
inline constexpr Color critical_border = palette::critical;
inline constexpr Color app = palette::accent;
inline constexpr Color summary = palette::text;
inline constexpr Color body = palette::text_muted;

// Content
inline constexpr const char *app_fallback = "Notification";

// Timing
inline constexpr std::chrono::milliseconds hang_time{1500};

} // namespace astralia::notification_config
