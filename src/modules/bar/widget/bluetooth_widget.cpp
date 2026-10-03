#include "config/bar_config.h"

#include "modules/bar/widget/bluetooth_widget.h"

#include "render/icons.h"

namespace astralia {

const char *bluetooth_icon(const BluetoothStatus &status) {
    if (!status.powered) {
        return icon::bluetooth_off;
    }
    return status.connected ? icon::bluetooth_connected : icon::bluetooth_on;
}

std::string bluetooth_label(const BluetoothStatus &status) {
    if (!status.present) {
        return "";
    }
    if (!status.powered) {
        return bar_config::bluetooth_disabled_label;
    }
    if (status.connected && !status.device.empty()) {
        return status.device;
    }
    return bar_config::bluetooth_idle_label;
}

void BluetoothWidget::update(const BluetoothStatus &status) {
    set_visible(status.present);
    set_icon(bluetooth_icon(status));
    set_label(bluetooth_label(status));
}

} // namespace astralia
