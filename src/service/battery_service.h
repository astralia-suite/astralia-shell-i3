#pragma once

#include <memory>
#include <sdbus-c++/sdbus-c++.h>

#include "core/dbus.h"
#include "core/signal.h"

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
    explicit BatteryService(SystemBus &bus);

    const BatteryStatus &status() const { return status_; }

    Signal<> changed;

  private:
    bool refresh();

    SystemBus &bus_;
    BatteryStatus status_;
    std::unique_ptr<sdbus::IProxy> device_;
    sdbus::Slot match_;
};

} // namespace astralia
