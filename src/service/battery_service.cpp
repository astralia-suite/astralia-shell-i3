#include <cmath>
#include <cstdint>
#include <string>

#include "service/battery_service.h"

namespace astralia {

namespace {

const std::string upower_service = "org.freedesktop.UPower";
const std::string display_device = "/org/freedesktop/UPower/devices/DisplayDevice";
const std::string device_interface = "org.freedesktop.UPower.Device";

// UPower device states
constexpr uint32_t state_charging = 1;
constexpr uint32_t state_fully_charged = 4;

} // namespace

BatteryService::BatteryService(SystemBus &bus)
    : bus_(bus), device_(bus_.proxy(upower_service, display_device)) {
    refresh();
    match_ = bus_.add_match("type='signal',sender='org.freedesktop.UPower',"
                            "path='/org/freedesktop/UPower/devices/DisplayDevice'",
                            [this] {
                                if (refresh()) {
                                    changed.emit();
                                }
                            });
}

bool BatteryService::refresh() {
    if (!device_) {
        return false;
    }
    BatteryStatus next;
    next.present =
        dbus_property<bool>(device_.get(), device_interface, "IsPresent").value_or(false);
    if (next.present) {
        double percent =
            dbus_property<double>(device_.get(), device_interface, "Percentage").value_or(0);
        next.percent = static_cast<int>(std::lround(percent));
        uint32_t state =
            dbus_property<uint32_t>(device_.get(), device_interface, "State").value_or(0);
        next.charging = state == state_charging;
        next.full = state == state_fully_charged;
    }
    if (next == status_) {
        return false;
    }
    status_ = next;
    return true;
}

} // namespace astralia
