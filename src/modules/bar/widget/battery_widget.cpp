#include <format>

#include "modules/bar/widget/battery_widget.h"

#include "render/icons.h"

namespace astralia {

const char *battery_icon(const BatteryStatus &status) {
    if (status.full) {
        return icon::plugged_in;
    }
    if (status.charging) {
        return icon::battery_charging;
    }
    if (status.percent <= 25) {
        return icon::battery1;
    }
    if (status.percent <= 50) {
        return icon::battery2;
    }
    if (status.percent <= 75) {
        return icon::battery3;
    }
    return icon::battery4;
}

std::string battery_label(const BatteryStatus &status) {
    if (status.full) {
        return "Plugged in";
    }
    return std::format("{}%", status.percent);
}

void BatteryWidget::update(const BatteryStatus &status) {
    set_visible(status.present);
    set_icon(battery_icon(status));
    set_label(status.present ? battery_label(status) : "");
}

} // namespace astralia
