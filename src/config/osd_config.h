#pragma once

#include <chrono>

#include "render/palette.h"

namespace astralia::osd_config {

// Surface
inline constexpr int width = 300;
inline constexpr int height = 50;
inline constexpr int margin_bottom = 30;
inline constexpr double radius = height / 2.0;
inline constexpr double border_width = metrics::border_thin;

// Content
inline constexpr double content_margin = height / 2.0;
inline constexpr double bar_margin = 10.0;
inline constexpr double label_width = 44.0;
inline constexpr double icon_size = 18.0;
inline constexpr double track_height = 6.0;

// Typography
inline constexpr const char *icon_font = "tabler-icons 18px";
inline constexpr const char *label_font = "Comic Shanns Mono 15";

// Timing
inline constexpr std::chrono::milliseconds visible_for{2000};
inline constexpr std::chrono::milliseconds ready_delay{1000};

} // namespace astralia::osd_config
