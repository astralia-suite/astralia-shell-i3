#include <format>

#include "modules/bar/widget/brightness_widget.h"

#include "render/icons.h"

namespace astralia {

void BrightnessWidget::update(const BrightnessService &brightness) {
    set_visible(brightness.available());
    if (!visible()) {
        return;
    }
    int percent = brightness.percent();
    set_icon(icon::brightness_threshold(percent));
    set_label(std::format("{}%", percent));
}

} // namespace astralia
