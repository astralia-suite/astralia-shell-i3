#include <algorithm>
#include <array>
#include <numbers>
#include <utility>

#include "config/settings_config.h"

#include "modules/settings/widgets.h"

#include "render/icons.h"
#include "render/text.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

constexpr std::array<const char *, cfg::tab_count> tab_icons{icon::layout_navbar, icon::device_desktop, icon::wallpaper};

void draw_centered_line(cairo_t *cr, const char *font, std::string_view text, const PanelRect &column, double top, double height, const Color &color) {
    int width = std::min(panel_text_width(font, text), static_cast<int>(column.w));
    panel_draw_text(cr, font, text, column.x + (column.w - width) / 2.0, top, height, static_cast<int>(column.w), color);
}

} // namespace

void settings_draw_chips(cairo_t *cr, const PanelRect &area, const std::optional<std::string> &default_key, const std::vector<Output> &outputs, const std::string &selected, int action, std::vector<PanelHit> &hits) {
    std::vector<std::string> keys;
    std::vector<std::string> labels;
    if (default_key) {
        keys.push_back(*default_key);
        labels.push_back(cfg::default_label);
    }
    for (const Output &output : outputs) {
        if (output.name.empty()) {
            continue;
        }
        keys.push_back(output.name);
        labels.push_back(output.name);
    }
    std::vector<PanelRect> rects = settings_chip_rects(keys.size(), area);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        settings_draw_choice(cr, rects[i], labels[i], keys[i] == selected, action, keys[i], hits);
    }
}

void settings_draw_choice(cairo_t *cr, const PanelRect &rect, std::string_view label, bool active, int action, std::string tag, std::vector<PanelHit> &hits) {
    set_source(cr, palette::lavender_alpha20);
    rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, cfg::chip_radius);
    cairo_fill(cr);
    if (active) {
        set_source(cr, palette::accent_alt);
        cairo_set_line_width(cr, panel_config::border_width);
        rounded_rect(cr, rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, cfg::chip_radius);
        cairo_stroke(cr);
    }
    auto inner = static_cast<int>(rect.w - 2 * cfg::chip_padding);
    int width = std::min(panel_text_width(panel_config::font, label), inner);
    panel_draw_text(cr, panel_config::font, label, rect.x + (rect.w - width) / 2.0, rect.y, rect.h, inner, palette::text);
    hits.push_back({rect, action, std::move(tag)});
}

void settings_draw_card(cairo_t *cr, const PanelRect &rect) {
    constexpr double inset = metrics::border_thick / 2.0;
    set_source(cr, palette::overlay);
    rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, metrics::radius_md);
    cairo_fill(cr);
    set_source(cr, palette::accent);
    cairo_set_line_width(cr, metrics::border_thick);
    rounded_rect(cr, rect.x + inset, rect.y + inset, rect.w - 2 * inset, rect.h - 2 * inset, metrics::radius_md - inset);
    cairo_stroke(cr);
}

void settings_draw_profile(cairo_t *cr, const SettingsGeometry &geometry, std::string_view name, std::string_view uptime) {
    const PanelRect &area = geometry.profile;
    double cx = area.x + area.w / 2.0;
    double cy = area.y + cfg::profile_top_padding + cfg::avatar_size / 2.0;
    set_source(cr, palette::text_alpha04);
    cairo_arc(cr, cx, cy, cfg::avatar_size / 2.0, 0.0, 2.0 * std::numbers::pi);
    cairo_fill(cr);
    set_source(cr, palette::accent);
    cairo_set_line_width(cr, cfg::avatar_border);
    cairo_arc(cr, cx, cy, cfg::avatar_size / 2.0 - cfg::avatar_border / 2.0, 0.0, 2.0 * std::numbers::pi);
    cairo_stroke(cr);
    Text glyph(panel_config::icon_font);
    glyph.set(icon::user);
    set_source(cr, palette::text);
    glyph.draw_ink_centered(cr, cx, cy);

    if (geometry.expanded) {
        double top = area.y + cfg::profile_top_padding + cfg::avatar_size + cfg::profile_label_gap;
        draw_centered_line(cr, panel_config::font, name, area, top, cfg::profile_name_height, palette::text);
        draw_centered_line(cr, panel_config::small_font, uptime, area, top + cfg::profile_name_height + cfg::profile_line_gap, cfg::profile_uptime_height, palette::text_dim);
    }
    set_source(cr, palette::text_alpha11);
    cairo_rectangle(cr, area.x, area.y + area.h, area.w, 1.0);
    cairo_fill(cr);
}

void settings_draw_rail(cairo_t *cr, const SettingsGeometry &geometry, std::size_t selected) {
    const PanelRect &rail = geometry.rail;
    set_source(cr, palette::text_alpha04);
    rounded_rect(cr, rail.x, rail.y, rail.w, rail.h, metrics::radius_md);
    cairo_fill(cr);
    for (std::size_t i = 0; i < cfg::tab_count; ++i) {
        PanelRect rect = settings_tab_rect(geometry, i);
        bool active = i == selected;
        const Color &color = active ? palette::accent : palette::text_dim;
        if (active) {
            set_source(cr, palette::accent_alpha19);
            rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, metrics::radius_sm);
            cairo_fill(cr);
        }
        double icon_x = rect.x + cfg::rail_padding;
        int icon_width = panel_text_width(panel_config::icon_font, tab_icons[i]);
        Text glyph(panel_config::icon_font);
        glyph.set(tab_icons[i]);
        set_source(cr, color);
        glyph.draw_ink_centered(cr, icon_x + icon_width / 2.0, rect.y + rect.h / 2.0);
        if (geometry.expanded) {
            double label_x = icon_x + icon_width + cfg::tab_icon_label_gap;
            panel_draw_text(cr, panel_config::font, cfg::tab_labels[i], label_x, rect.y, rect.h, static_cast<int>(rect.x + rect.w - cfg::rail_padding - label_x), color);
        }
    }
}

void settings_draw_toggle_tile(cairo_t *cr, const PanelRect &rect, std::string_view label, bool on, int action, std::string tag, std::vector<PanelHit> &hits) {
    constexpr double inset = cfg::tile_border / 2.0;
    set_source(cr, palette::text_alpha04);
    rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, cfg::tile_radius);
    cairo_fill(cr);
    set_source(cr, palette::text_alpha07);
    cairo_set_line_width(cr, cfg::tile_border);
    rounded_rect(cr, rect.x + inset, rect.y + inset, rect.w - 2 * inset, rect.h - 2 * inset, cfg::tile_radius - inset);
    cairo_stroke(cr);
    panel_draw_text(cr, panel_config::font, label, rect.x + cfg::tile_inset, rect.y, rect.h, static_cast<int>(rect.w - 2 * cfg::tile_inset - panel_config::toggle_width), palette::text_alpha85);
    panel_draw_toggle(cr, rect.x + rect.w - cfg::tile_inset - panel_config::toggle_width, rect.y + (rect.h - panel_config::toggle_height) / 2.0, on, cfg::toggle_knob, cfg::toggle_knob_inset);
    hits.push_back({rect, action, std::move(tag)});
}

} // namespace astralia
