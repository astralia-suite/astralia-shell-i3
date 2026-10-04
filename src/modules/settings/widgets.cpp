#include <algorithm>
#include <utility>

#include "config/settings_config.h"

#include "modules/settings/layout.h"
#include "modules/settings/widgets.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

}

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
        const PanelRect &rect = rects[i];
        set_source(cr, palette::lavender_alpha20);
        rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, cfg::chip_radius);
        cairo_fill(cr);
        if (keys[i] == selected) {
            set_source(cr, palette::accent_alt);
            cairo_set_line_width(cr, panel_config::border_width);
            rounded_rect(cr, rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, cfg::chip_radius);
            cairo_stroke(cr);
        }
        auto inner = static_cast<int>(rect.w - 2 * cfg::chip_padding);
        int width = std::min(panel_text_width(panel_config::font, labels[i]), inner);
        panel_draw_text(cr, panel_config::font, labels[i], rect.x + (rect.w - width) / 2.0, rect.y, rect.h, inner, palette::text);
        hits.push_back({rect, action, keys[i]});
    }
}

void settings_draw_toggle_tile(cairo_t *cr, const PanelRect &rect, std::string_view label, bool on, int action, std::string tag, std::vector<PanelHit> &hits) {
    set_source(cr, palette::text_alpha06);
    rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, cfg::tile_radius);
    cairo_fill(cr);
    panel_draw_text(cr, panel_config::font, label, rect.x + cfg::tile_inset, rect.y, rect.h, static_cast<int>(rect.w - 2 * cfg::tile_inset - panel_config::toggle_width), palette::text);
    panel_draw_toggle(cr, rect.x + rect.w - cfg::tile_inset - panel_config::toggle_width, rect.y + (rect.h - panel_config::toggle_height) / 2.0, on);
    hits.push_back({rect, action, std::move(tag)});
}

} // namespace astralia
