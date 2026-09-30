#pragma once

#include <functional>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

#include "core/dbus.h"

#include "service/network_service.h"

namespace astralia {

struct BluetoothStatus {
    bool present = false;
    bool powered = false;
    bool connected = false;
    std::string device;

    bool operator==(const BluetoothStatus &) const = default;
};

std::vector<StatusMessage> bluetooth_changes(const BluetoothStatus &prev, const BluetoothStatus &next);

class BluetoothService {
  public:
    BluetoothService(SystemBus &bus, std::function<void()> on_change, NotifyFn notify);

    const BluetoothStatus &status() const { return status_; }

  private:
    bool refresh();

    SystemBus &bus_;
    std::function<void()> on_change_;
    NotifyFn notify_;
    BluetoothStatus status_;
    std::unique_ptr<sdbus::IProxy> root_;
    sdbus::Slot match_;
};

} // namespace astralia
