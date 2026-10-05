#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string_view>

namespace astralia::settings_config {

// Config file
inline constexpr const char *file = "astralia-shell/settings.conf";
inline constexpr const char *default_wallpaper_dir = "~/Pictures";
inline constexpr const char *default_text =
    "# astralia-shell settings\n"
    "# bar, osd, notifications: on or off.\n"
    "# Prefix a key with an output name to override it, e.g. HDMI-1.osd = off\n"
    "# bar_style: continuous or okinami.\n"
    "bar = on\n"
    "osd = on\n"
    "notifications = on\n"
    "bar_style = continuous\n"
    "wallpaper_dir = ~/Pictures\n";
inline constexpr const char *wallpaper_dir_key = "wallpaper_dir";
inline constexpr const char *bar_style_key = "bar_style";

// Window
inline constexpr int card_max_width = 920;
inline constexpr int card_max_height = 680;
inline constexpr int card_margin = 40;
inline constexpr double card_padding = 20.0;
inline constexpr double card_radius = 14.0;

// Header
inline constexpr double header_divider_gap = 4.0;
inline constexpr double content_gap = 8.0;

// Tabs
inline constexpr std::size_t tab_count = 3;
inline constexpr std::array<const char *, tab_count> tab_labels{"Bar", "Displays", "Wallpaper"};
inline constexpr double rail_expanded_width = 200.0;
inline constexpr double rail_collapsed_width = 64.0;
inline constexpr int rail_collapse_breakpoint = 700;
inline constexpr double rail_padding = 10.0;
inline constexpr double rail_divider_gap = 16.0;
inline constexpr double tab_height = 36.0;
inline constexpr double tab_gap = 4.0;
inline constexpr double tab_icon_label_gap = 10.0;

// Profile
inline constexpr double avatar_size = 40.0;
inline constexpr double avatar_border = 2.0;
inline constexpr double profile_top_padding = 4.0;
inline constexpr double profile_label_gap = 10.0;
inline constexpr double profile_line_gap = 2.0;
inline constexpr double profile_name_height = 18.0;
inline constexpr double profile_uptime_height = 16.0;
inline constexpr double profile_bottom_padding = 12.0;
inline constexpr double profile_divider_gap = 12.0;

// Content
inline constexpr double section_gap = 12.0;
inline constexpr double chip_height = 35.0;
inline constexpr double chip_gap = 6.0;
inline constexpr double chip_padding = 16.0;
inline constexpr double chip_radius = 6.0;
inline constexpr double tile_height = 48.0;
inline constexpr double tile_gap = 8.0;
inline constexpr double tile_inset = 12.0;
inline constexpr double tile_border = 2.0;
inline constexpr double toggle_knob = 14.0;
inline constexpr double toggle_knob_inset = 3.0;
inline constexpr double tile_radius = 6.0;
inline constexpr const char *default_label = "Default";
inline constexpr const char *override_label = "Override default settings";

// Wallpaper folder bar
inline constexpr double folder_bar_height = 40.0;
inline constexpr double folder_label_width = 64.0;
inline constexpr double folder_field_height = 28.0;
inline constexpr double folder_button_width = 72.0;
inline constexpr double folder_gap = 10.0;
inline constexpr const char *folder_label = "Folder";
inline constexpr const char *reset_label = "Reset";
inline constexpr const char *empty_label = "No images in this folder";

// Wallpaper grid
inline constexpr double thumb_size = 115.0;
inline constexpr double thumb_gap = 15.0;
inline constexpr int thumb_columns = 5;
inline constexpr double thumb_radius = 8.0;
inline constexpr double thumb_border = 3.0;
inline constexpr double scroll_step = 60.0;
inline constexpr std::size_t max_images = 400;
inline constexpr std::size_t thumb_cache_limit = 80;
inline constexpr std::array<std::string_view, 6> image_extensions{".png", ".jpg", ".jpeg", ".jfif", ".bmp", ".svg"};

// Repaint
inline constexpr std::chrono::milliseconds repaint_batch{50};
inline constexpr std::chrono::milliseconds repaint_idle = std::chrono::hours(1);
inline constexpr std::chrono::milliseconds slow_paint{16};

} // namespace astralia::settings_config
