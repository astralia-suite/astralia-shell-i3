#pragma once

#include "render/palette.h"

namespace astralia::polkit_config {

// Card geometry
inline constexpr double card_width = 480.0;
inline constexpr double card_pad = 30.0;
inline constexpr double card_radius = 20.0;
inline constexpr double border_width = 5.0;
inline constexpr double spacing = 16.0;
inline constexpr double field_height = 55.0;
inline constexpr double field_radius = 27.0;
inline constexpr double dot_margin = 8.0;
inline constexpr int dot_size = 16;

// Line heights
inline constexpr double title_line_height = 20.0;
inline constexpr double message_line_height = 16.0;
inline constexpr double info_line_height = 14.0;

// Typography
inline constexpr const char *title_font = "Comic Shanns Mono Bold 15px";
inline constexpr const char *message_font = "Comic Shanns Mono 12px";
inline constexpr const char *field_font = "Comic Shanns Mono 12px";
inline constexpr const char *info_font = "Comic Shanns Mono 11px";

// Colors
inline constexpr Color background = palette::overlay;
inline constexpr Color border = palette::accent;
inline constexpr Color title = palette::text;
inline constexpr Color muted = palette::text_muted;
inline constexpr Color field_background = palette::field_bg;
inline constexpr Color dot = palette::text;
inline constexpr Color error = palette::critical;

// Content
inline constexpr const char *title_text = "Authentication Required";
inline constexpr const char *password_placeholder = "Password";
inline constexpr const char *authenticating_text = "Authenticating...";
inline constexpr const char *error_text = "Skill Issue";
inline constexpr const char *echo_file = "electro.png";

} // namespace astralia::polkit_config
