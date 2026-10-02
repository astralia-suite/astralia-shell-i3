#include "config/bar_config.h"

#include "core/icons.h"

#include "modules/bar/widget/control_center_widget.h"

namespace astralia {

ControlCenterWidget::ControlCenterWidget() : icon_(bar_config::icon_font) {
    icon_.set(icon::adjustments);
}

int ControlCenterWidget::width() const {
    return icon_.width();
}

void ControlCenterWidget::draw(cairo_t *cr, double x, int top, int height) const {
    icon_.draw_centered(cr, x, top, height);
}

} // namespace astralia
