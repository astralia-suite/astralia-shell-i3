#include <numbers>

#include "config/bar_config.h"

#include "modules/bar/workspace_widget.h"

namespace astralia {

namespace {

int pill_width(const WorkspaceStatus &status, uint32_t index) {
    return index == status.current ? bar_config::pill_active_width : bar_config::pill_width;
}

void fill_pill(cairo_t *cr, double x, double y, double width, double height) {
    double radius = height / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -std::numbers::pi / 2,
              std::numbers::pi / 2);
    cairo_arc(cr, x + radius, y + radius, radius, std::numbers::pi / 2, 3 * std::numbers::pi / 2);
    cairo_close_path(cr);
    cairo_fill(cr);
}

} // namespace

int workspace_row_width(const WorkspaceStatus &status) {
    int width = 0;
    for (uint32_t i = 0; i < status.count; ++i) {
        width += pill_width(status, i) + (i > 0 ? bar_config::pill_spacing : 0);
    }
    return width;
}

std::optional<uint32_t> workspace_at(const WorkspaceStatus &status, int offset) {
    int start = 0;
    for (uint32_t i = 0; i < status.count; ++i) {
        int end = start + pill_width(status, i) + bar_config::pill_spacing;
        if (offset >= start - bar_config::pill_spacing / 2 &&
            offset < end - bar_config::pill_spacing / 2) {
            return i;
        }
        start = end;
    }
    return std::nullopt;
}

void draw_workspace_row(cairo_t *cr, const WorkspaceStatus &status, double x, int top, int height) {
    double y = top + (height - bar_config::pill_height) / 2.0;
    for (uint32_t i = 0; i < status.count; ++i) {
        const Color &color = i == status.current           ? bar_config::pill_active
                             : (status.occupied >> i) & 1u ? bar_config::pill_occupied
                                                           : bar_config::pill_inactive;
        cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
        int width = pill_width(status, i);
        fill_pill(cr, x, y, width, bar_config::pill_height);
        x += width + bar_config::pill_spacing;
    }
}

} // namespace astralia
