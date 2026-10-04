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
    "bar = on\n"
    "osd = on\n"
    "notifications = on\n"
    "wallpaper_dir = ~/Pictures\n";
inline constexpr const char *wallpaper_dir_key = "wallpaper_dir";

// Window
inline constexpr int card_max_width = 860;
inline constexpr int card_max_height = 540;
inline constexpr int card_margin = 40;
inline constexpr double card_radius = 14.0;

// Tabs
inline constexpr std::size_t tab_count = 2;
inline constexpr std::array<const char *, tab_count> tab_labels{"Displays", "Wallpaper"};
inline constexpr double rail_width = 170.0;
inline constexpr double rail_padding = 12.0;
inline constexpr double rail_title_height = 44.0;
inline constexpr double tab_height = 36.0;
inline constexpr double tab_gap = 4.0;
inline constexpr double tab_text_inset = 12.0;

// Content
inline constexpr double content_padding = 20.0;
inline constexpr double section_gap = 12.0;
inline constexpr double chip_height = 35.0;
inline constexpr double chip_gap = 6.0;
inline constexpr double chip_padding = 16.0;
inline constexpr double chip_radius = 6.0;
inline constexpr double tile_height = 48.0;
inline constexpr double tile_gap = 8.0;
inline constexpr double tile_inset = 12.0;
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
