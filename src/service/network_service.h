#pragma once

#include <functional>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "core/dbus.h"

namespace astralia {

enum class NetworkKind { none,
                         ethernet,
                         wifi };

struct NetworkStatus {
    NetworkKind kind = NetworkKind::none;
    int strength = 0;
    std::string ssid;
    bool portal = false;

    bool operator==(const NetworkStatus &) const = default;
};

class NetworkService {
  public:
    NetworkService(SystemBus &bus, std::function<void()> on_change);

    const NetworkStatus &status() const { return status_; }

  private:
    bool refresh();

    SystemBus &bus_;
    std::function<void()> on_change_;
    NetworkStatus status_;
    std::unique_ptr<sdbus::IProxy> manager_;
    sdbus::Slot match_;
};

} // namespace astralia
