#include <algorithm>
#include <cmath>
#include <numbers>

#include "config/panel_config.h"

#include "render/icons.h"
#include "render/panel_chrome.h"
#include "render/text.h"

namespace astralia {

int slider_percent_at(int track_x, int track_width, int px) {
    if (track_width <= 0) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround((px - track_x) * 100.0 / track_width)), 0, 100);
}

int panel_clamp_scroll(int offset, int content_height, int visible_height) {
    return std::clamp(offset, 0, std::max(0, content_height - visible_height));
}

std::optional<PanelHit> panel_hit_at(const std::vector<PanelHit> &hits, double x, double y) {
    for (const PanelHit &hit : hits) {
        if (hit.rect.contains(x, y)) {
            return hit;
        }
    }
    return std::nullopt;
}

PanelRect panel_intersect(const PanelRect &a, const PanelRect &b) {
    double left = std::max(a.x, b.x);
    double top = std::max(a.y, b.y);
    double right = std::min(a.x + a.w, b.x + b.w);
    double bottom = std::min(a.y + a.h, b.y + b.h);
    return {left, top, std::max(0.0, right - left), std::max(0.0, bottom - top)};
}

void panel_draw_card(cairo_t *cr, double x, double y, double w, double h) {
    constexpr double inset = panel_config::border_width / 2.0;
    set_source(cr, panel_config::background);
    rounded_rect(cr, x, y, w, h, metrics::radius_md);
    cairo_fill(cr);
    set_source(cr, panel_config::border);
    cairo_set_line_width(cr, panel_config::border_width);
    rounded_rect(cr, x + inset, y + inset, w - 2 * inset, h - 2 * inset, metrics::radius_md - inset);
    cairo_stroke(cr);
}

int panel_draw_text(cairo_t *cr, const char *font, std::string_view text, double x, double top, double height, int max_width, const Color &color) {
    Text layout(font);
    layout.set(text);
    if (max_width > 0 && layout.width() > max_width) {
        layout.ellipsize(max_width);
    }
    set_source(cr, color);
    layout.draw_centered(cr, x, static_cast<int>(top), static_cast<int>(height));
    return layout.width();
}

int panel_text_width(const char *font, std::string_view text) {
    Text layout(font);
    layout.set(text);
    return layout.width();
}

PanelRect panel_draw_icon_button(cairo_t *cr, double x, double y, const char *icon, const Color &color) {
    constexpr double size = panel_config::button_size;
    set_source(cr, panel_config::button);
    cairo_arc(cr, x + size / 2.0, y + size / 2.0, size / 2.0, 0.0, 2.0 * std::numbers::pi);
    cairo_fill(cr);
    Text glyph(panel_config::icon_font);
    glyph.set(icon);
    set_source(cr, color);
    glyph.draw_ink_centered(cr, x + size / 2.0, y + size / 2.0);
    return {x, y, size, size};
}

PanelRect panel_draw_toggle(cairo_t *cr, double x, double y, bool on) {
    constexpr double w = panel_config::toggle_width;
    constexpr double h = panel_config::toggle_height;
    constexpr double knob = h - 4.0;
    set_source(cr, on ? palette::accent : palette::text_alpha20);
    rounded_rect(cr, x, y, w, h, h / 2.0);
    cairo_fill(cr);
    set_source(cr, palette::text);
    double knob_x = on ? x + w - 2.0 - knob : x + 2.0;
    cairo_arc(cr, knob_x + knob / 2.0, y + h / 2.0, knob / 2.0, 0.0, 2.0 * std::numbers::pi);
    cairo_fill(cr);
    return {x, y, w, h};
}

void panel_draw_slider(cairo_t *cr, const PanelRect &track, int percent, bool muted) {
    constexpr double h = panel_config::track_height;
    double top = track.y + (track.h - h) / 2.0;
    set_source(cr, panel_config::track);
    rounded_rect(cr, track.x, top, track.w, h, h / 2.0);
    cairo_fill(cr);
    if (percent > 0) {
        set_source(cr, muted ? palette::text_muted : panel_config::fill);
        rounded_rect(cr, track.x, top, track.w * std::min(percent, 100) / 100.0, h, h / 2.0);
        cairo_fill(cr);
    }
}

double panel_draw_header(cairo_t *cr, double width, std::string_view title, std::vector<PanelHit> &hits, int close_action) {
    constexpr double pad = panel_config::padding;
    constexpr double header = panel_config::header_height;
    panel_draw_text(cr, panel_config::title_font, title, pad, pad, header, 0, panel_config::foreground);
    double close_x = width - pad - panel_config::button_size;
    PanelRect close = panel_draw_icon_button(cr, close_x, pad + (header - panel_config::button_size) / 2.0, icon::close, panel_config::foreground);
    hits.push_back({close, close_action, {}});
    set_source(cr, panel_config::divider);
    cairo_rectangle(cr, pad, pad + header + panel_config::row_gap, width - 2 * pad, 1.0);
    cairo_fill(cr);
    return close_x - panel_config::row_gap;
}

double panel_content_top() {
    return panel_config::padding + panel_config::header_height + 2.0 * panel_config::row_gap + 1.0;
}

void panel_draw_section(cairo_t *cr, double y, double height, std::string_view label) {
    Text layout(panel_config::small_font);
    layout.set(label);
    set_source(cr, palette::text_dim);
    layout.draw(cr, panel_config::padding, y + height - layout.height());
}

void panel_draw_centered(cairo_t *cr, const PanelRect &rect, std::string_view text) {
    int width = panel_text_width(panel_config::font, text);
    panel_draw_text(cr, panel_config::font, text, rect.x + (rect.w - width) / 2.0, rect.y, rect.h, 0, palette::text_dim);
}

void panel_draw_device_row(cairo_t *cr, const PanelRect &rect, const char *icon, std::string_view title, std::string_view subtitle, const Color &background, const Color &foreground, double reserve_right) {
    constexpr double slot = panel_config::button_size + 8.0;
    set_source(cr, background);
    rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, metrics::radius_sm * 1.6);
    cairo_fill(cr);
    Text glyph(panel_config::icon_font);
    glyph.set(icon);
    set_source(cr, foreground);
    glyph.draw_ink_centered(cr, rect.x + slot / 2.0 + 2.0, rect.y + rect.h / 2.0);
    double text_x = rect.x + slot + 4.0;
    int text_w = static_cast<int>(rect.x + rect.w - reserve_right - 8.0 - text_x);
    if (subtitle.empty()) {
        panel_draw_text(cr, panel_config::font, title, text_x, rect.y, rect.h, text_w, foreground);
        return;
    }
    double half = rect.h / 2.0;
    panel_draw_text(cr, panel_config::font, title, text_x, rect.y + 3.0, half, text_w, foreground);
    panel_draw_text(cr, panel_config::small_font, subtitle, text_x, rect.y + half - 2.0, half, text_w, palette::text_dim);
}

double panel_confirm_height(double prompt_height) {
    return 2.0 * panel_config::padding + panel_config::label_height + prompt_height + panel_config::row_gap + panel_config::button_size + 4.0;
}

void panel_draw_confirm(cairo_t *cr, double y, double width, std::string_view title, std::string_view prompt, std::string_view confirm, std::vector<PanelHit> &hits, int cancel_action, int confirm_action, double prompt_height) {
    constexpr double pad = panel_config::padding;
    constexpr double label = panel_config::label_height;
    int inner = static_cast<int>(width - 2 * pad);
    panel_draw_card(cr, 0, y, width, panel_confirm_height(prompt_height));
    double top = y + pad;
    panel_draw_text(cr, panel_config::font, title, pad, top, label, inner, panel_config::foreground);
    top += label;
    panel_draw_text(cr, panel_config::small_font, prompt, pad, top, prompt_height, inner, palette::text_dim);
    top += prompt_height + panel_config::row_gap;
    double button_h = panel_config::button_size + 4.0;
    double button_w = (inner - panel_config::row_gap) / 2.0;
    PanelRect cancel{pad, top, button_w, button_h};
    PanelRect accept{pad + button_w + panel_config::row_gap, top, button_w, button_h};
    set_source(cr, panel_config::button);
    rounded_rect(cr, cancel.x, cancel.y, cancel.w, cancel.h, metrics::radius_sm);
    cairo_fill(cr);
    set_source(cr, palette::accent);
    rounded_rect(cr, accept.x, accept.y, accept.w, accept.h, metrics::radius_sm);
    cairo_fill(cr);
    int cancel_w = panel_text_width(panel_config::font, "Cancel");
    panel_draw_text(cr, panel_config::font, "Cancel", cancel.x + (cancel.w - cancel_w) / 2.0, cancel.y, cancel.h, 0, panel_config::foreground);
    int confirm_w = panel_text_width(panel_config::font, confirm);
    panel_draw_text(cr, panel_config::font, confirm, accept.x + (accept.w - confirm_w) / 2.0, accept.y, accept.h, 0, panel_config::foreground);
    hits.push_back({cancel, cancel_action, {}});
    hits.push_back({accept, confirm_action, {}});
}

} // namespace astralia
