#pragma once

#include <cairo.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "render/draw.h"
#include "render/palette.h"

namespace astralia::panel_config {

// Geometry
inline constexpr int padding = 14;
inline constexpr int header_height = 30;
inline constexpr int label_height = 24;
inline constexpr int row_gap = 6;
inline constexpr int button_size = 26;
inline constexpr int toggle_width = 36;
inline constexpr int toggle_height = 20;
inline constexpr int track_height = 6;
inline constexpr float border_width = metrics::border_thin;

// Typography
inline constexpr const char *title_font = "Comic Shanns Mono 15";
inline constexpr const char *font = "Comic Shanns Mono 12";
inline constexpr const char *small_font = "Comic Shanns Mono 10";
inline constexpr const char *icon_font = "tabler-icons 15";

} // namespace astralia::panel_config

namespace astralia {

struct PanelRect {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;

    bool contains(double px, double py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct PanelHit {
    PanelRect rect;
    int action = 0;
    std::string tag;
};

int panel_clamp_scroll(int offset, int content_height, int visible_height);
std::optional<PanelHit> panel_hit_at(const std::vector<PanelHit> &hits, double x, double y);
PanelRect panel_intersect(const PanelRect &a, const PanelRect &b);

void panel_draw_card(cairo_t *cr, double x, double y, double w, double h);
int panel_draw_text(cairo_t *cr, const char *font, std::string_view text, double x, double top, double height, int max_width, const Color &color);
int panel_text_width(const char *font, std::string_view text);
PanelRect panel_draw_icon_button(cairo_t *cr, double x, double y, const char *icon, const Color &color);
PanelRect panel_draw_toggle(cairo_t *cr, double x, double y, bool on, double knob = panel_config::toggle_height - 4.0, double inset = 2.0);
double panel_draw_header(cairo_t *cr, double width, std::string_view title, std::vector<PanelHit> &hits, int close_action);
double panel_content_top();
void panel_draw_section(cairo_t *cr, double y, double height, std::string_view label);
void panel_draw_centered(cairo_t *cr, const PanelRect &rect, std::string_view text);
void panel_draw_device_row(cairo_t *cr, const PanelRect &rect, const char *icon, std::string_view title, std::string_view subtitle, const Color &background, const Color &foreground, double reserve_right);
double panel_confirm_height(double prompt_height = panel_config::label_height);
void panel_draw_confirm(cairo_t *cr, double y, double width, std::string_view title, std::string_view prompt, std::string_view confirm, std::vector<PanelHit> &hits, int cancel_action, int confirm_action, double prompt_height = panel_config::label_height);

} // namespace astralia
