#include <algorithm>
#include <cmath>

#include "config/bar_config.h"

#include "modules/bar/widget/dock_widget.h"

namespace astralia {

std::vector<DockEntry> dock_entries(const I3Tree &tree, uint32_t workspace) {
    std::vector<const I3Window *> matched;
    for (const I3Window &window : tree.windows) {
        if (window.workspace == workspace) {
            matched.push_back(&window);
        }
    }
    std::stable_sort(matched.begin(), matched.end(), [](const I3Window *a, const I3Window *b) { return a->x < b->x; });
    std::vector<DockEntry> entries;
    entries.reserve(matched.size());
    for (const I3Window *window : matched) {
        entries.push_back({window->window_class, window->focused});
    }
    return entries;
}

bool DockWidget::update(const I3Tree &tree, uint32_t workspace) {
    std::vector<DockEntry> next = dock_entries(tree, workspace);
    if (next == entries_) {
        return false;
    }
    entries_ = std::move(next);
    return true;
}

int DockWidget::width() const {
    if (entries_.empty()) {
        return 0;
    }
    int count = static_cast<int>(entries_.size());
    return count * bar_config::dock_icon_size + (count - 1) * bar_config::dock_icon_spacing;
}

cairo_surface_t *DockWidget::icon_for(const std::string &window_class) {
    auto it = icons_.find(window_class);
    if (it == icons_.end()) {
        it = icons_.emplace(window_class, load_app_icon(resolve_window_icon_path(window_class), bar_config::dock_icon_size)).first;
    }
    return it->second.get();
}

void DockWidget::draw(cairo_t *cr, double x, int top, int height) {
    double y = std::round(top + (height - bar_config::dock_icon_size) / 2.0);
    for (const DockEntry &entry : entries_) {
        if (cairo_surface_t *icon = icon_for(entry.window_class)) {
            double w = cairo_image_surface_get_width(icon);
            double h = cairo_image_surface_get_height(icon);
            cairo_set_source_surface(cr, icon, std::round(x + (bar_config::dock_icon_size - w) / 2.0), std::round(y + (bar_config::dock_icon_size - h) / 2.0));
            cairo_paint_with_alpha(cr, entry.focused ? bar_config::dock_focused_opacity : bar_config::dock_unfocused_opacity);
        }
        x += bar_config::dock_icon_size + bar_config::dock_icon_spacing;
    }
}

} // namespace astralia
