#include <algorithm>
#include <array>
#include <string_view>

#include "config/settings_config.h"

#include "modules/settings/displays_tab.h"
#include "modules/settings/widgets.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

constexpr std::array<std::string_view, feature_count> feature_labels{"Bar", "OSD", "Notifications"};

} // namespace

void DisplaysTab::paint(cairo_t *cr, const PanelRect &area, std::vector<PanelHit> &hits) {
    const std::vector<Output> &outputs = services_.outputs.outputs();
    if (!selected_.empty() && std::ranges::none_of(outputs, [&](const Output &output) { return output.name == selected_; })) {
        selected_.clear();
    }
    settings_draw_chips(cr, area, "", outputs, selected_, chip, hits);
    double y = area.y + cfg::chip_height + cfg::section_gap;

    bool overridden = services_.settings.overridden(selected_);
    if (!selected_.empty()) {
        settings_draw_toggle_tile(cr, {area.x, y, area.w, cfg::tile_height}, cfg::override_label, overridden, override_toggle, {}, hits);
        y += cfg::tile_height + cfg::tile_gap;
    }
    if (selected_.empty() || overridden) {
        for (std::size_t i = 0; i < feature_count; ++i) {
            bool on = services_.settings.enabled(static_cast<Feature>(i), selected_);
            settings_draw_toggle_tile(cr, {area.x, y, area.w, cfg::tile_height}, feature_labels[i], on, feature_toggle, std::to_string(i), hits);
            y += cfg::tile_height + cfg::tile_gap;
        }
    }
}

bool DisplaysTab::click(const std::optional<PanelHit> &hit) {
    if (!hit) {
        return false;
    }
    switch (hit->action) {
    case chip:
        selected_ = hit->tag;
        return true;
    case override_toggle:
        services_.settings.set_override(selected_, !services_.settings.overridden(selected_));
        return true;
    case feature_toggle: {
        auto feature = static_cast<Feature>(std::stoul(hit->tag));
        services_.settings.set(feature, selected_, !services_.settings.enabled(feature, selected_));
        return true;
    }
    default:
        return false;
    }
}

} // namespace astralia
