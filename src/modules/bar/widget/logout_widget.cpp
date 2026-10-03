#include "config/bar_config.h"

#include "modules/bar/widget/logout_widget.h"

#include "render/icons.h"

namespace astralia {

LogoutWidget::LogoutWidget() : icon_(bar_config::icon_font), label_(bar_config::font) {
    icon_.set(icon::power);
    label_.set(bar_config::logout_label);
}

bool LogoutWidget::hover(bool hovered) {
    if (hovered == hovered_) {
        return false;
    }
    hovered_ = hovered;
    return true;
}

int LogoutWidget::width() const {
    return icon_.width() + (hovered_ ? bar_config::label_gap + label_.width() : 0);
}

void LogoutWidget::draw(cairo_t *cr, double x, int top, int height) const {
    icon_.draw_centered(cr, x, top, height);
    if (hovered_) {
        label_.draw_centered(cr, x + icon_.width() + bar_config::label_gap, top, height);
    }
}

} // namespace astralia
