#include <format>

#include "config/bar_config.h"

#include "core/icons.h"

#include "modules/bar/widget/status_widget.h"

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

const char *network_icon(const NetworkStatus &status) {
    if (status.portal) {
        return icon::lock;
    }
    switch (status.kind) {
    case NetworkKind::ethernet:
        return icon::router;
    case NetworkKind::wifi:
        if (status.strength > 75) {
            return icon::wifi;
        }
        if (status.strength > 50) {
            return icon::wifi2;
        }
        if (status.strength > 25) {
            return icon::wifi1;
        }
        return icon::wifi0;
    case NetworkKind::none:
        break;
    }
    return icon::wifi_off;
}

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

StatusWidget::Item::Item() : icon(bar_config::icon_font), label(bar_config::font) {}

int StatusWidget::Item::width(bool show_label) const {
    int label_width = show_label ? label.width() : 0;
    return icon.width() + (label_width > 0 ? bar_config::label_gap + label_width : 0);
}

void StatusWidget::update(const BluetoothStatus &bluetooth, const NetworkStatus &network,
                          const BatteryStatus &battery) {
    auto &[bluetooth_item, network_item, battery_item] = items_;
    bluetooth_item.visible = bluetooth.present;
    bluetooth_item.icon.set(bluetooth_icon(bluetooth));
    bluetooth_item.label.set(bluetooth_label(bluetooth));
    network_item.icon.set(network_icon(network));
    network_item.label.set(network.kind == NetworkKind::wifi ? network.ssid : "");
    battery_item.visible = battery.present;
    battery_item.icon.set(battery_icon(battery));
    battery_item.label.set(battery.present ? battery_label(battery) : "");
}

bool StatusWidget::hover(std::optional<int> offset) {
    std::optional<std::size_t> next;
    int start = 0;
    for (std::size_t i = 0; offset && i < items_.size(); ++i) {
        if (!items_[i].visible) {
            continue;
        }
        int end = start + items_[i].width(i == hovered_) + bar_config::item_gap;
        if (*offset >= start - bar_config::item_gap / 2 &&
            *offset < end - bar_config::item_gap / 2) {
            next = i;
            break;
        }
        start = end;
    }
    if (next == hovered_) {
        return false;
    }
    hovered_ = next;
    return true;
}

int StatusWidget::width() const {
    int width = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].visible) {
            width += (width > 0 ? bar_config::item_gap : 0) + items_[i].width(i == hovered_);
        }
    }
    return width;
}

void StatusWidget::draw(cairo_t *cr, double x, int top, int height) const {
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Item &item = items_[i];
        if (!item.visible) {
            continue;
        }
        item.icon.draw_centered(cr, x, top, height);
        if (i == hovered_ && item.label.width() > 0) {
            item.label.draw_centered(cr, x + item.icon.width() + bar_config::label_gap, top,
                                     height);
        }
        x += item.width(i == hovered_) + bar_config::item_gap;
    }
}

} // namespace astralia
