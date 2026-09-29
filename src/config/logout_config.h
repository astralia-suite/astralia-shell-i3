#pragma once

#include <array>
#include <numbers>

#include "core/palette.h"

namespace astralia {

struct LogoutAction {
    const char *glyph;
    const char *command;
};

namespace logout_config {

// Geometry
inline constexpr int button_count = 8;
inline constexpr double button_size = 110.0;
inline constexpr double button_corner_radius = button_size / 5.0;
inline constexpr double ring_radius = 300.0;
inline constexpr double border_width = 5.0;
inline constexpr double highlight_scale = 1.05;
inline constexpr int logo_size = 250;

// Ring angles
inline constexpr double start_angle = -std::numbers::pi / 2.0;
inline constexpr double step_angle = 2.0 * std::numbers::pi / button_count;

// Typography
inline constexpr const char *glyph_font = "Yuji Mai 55px";

// Colors
inline constexpr Color button_fill = palette::field_bg;
inline constexpr Color border = palette::accent;
inline constexpr Color highlight_border = palette::accent_alt;
inline constexpr Color glyph = palette::text;

// Logo
inline constexpr const char *logo_file = "logo.png";

// Actions
inline constexpr std::array<LogoutAction, button_count> actions{{
    {"劍", "systemctl poweroff"},
    {"光", "systemctl reboot"},
    {"如", ""},
    {"我", "systemctl reboot --firmware-setup"},
    {"斬", ""},
    {"盡", ""},
    {"蕪", ""},
    {"雜", ""},
}};

} // namespace logout_config

} // namespace astralia
