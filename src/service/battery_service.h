#pragma once

#include <functional>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>

#include "core/dbus.h"

namespace astralia {

struct BatteryStatus {
    bool present = false;
    int percent = 0;
    bool charging = false;
    bool full = false;

    bool operator==(const BatteryStatus &) const = default;
};

class BatteryService {
  public:
    BatteryService(SystemBus &bus, std::function<void()> on_change);

    const BatteryStatus &status() const { return status_; }

  private:
    bool refresh();

    SystemBus &bus_;
    std::function<void()> on_change_;
    BatteryStatus status_;
    std::unique_ptr<sdbus::IProxy> device_;
    sdbus::Slot match_;
};

} // namespace astralia
