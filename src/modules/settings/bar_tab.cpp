#include "config/bar_config.h"

#include "modules/settings/bar_tab.h"
#include "modules/settings/layout.h"
#include "modules/settings/widgets.h"

namespace astralia {

void BarTab::paint(cairo_t *cr, const PanelRect &area, std::vector<PanelHit> &hits) {
    std::vector<PanelRect> rects = settings_chip_rects(bar_config::style_count, area);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        bool active = services_.settings.bar_style() == static_cast<BarStyle>(i);
        settings_draw_choice(cr, rects[i], bar_config::style_labels[i], active, style, std::to_string(i), hits);
    }
}

bool BarTab::click(const std::optional<PanelHit> &hit) {
    if (!hit || hit->action != style) {
        return false;
    }
    services_.settings.set_bar_style(static_cast<BarStyle>(std::stoul(hit->tag)));
    return true;
}

} // namespace astralia
