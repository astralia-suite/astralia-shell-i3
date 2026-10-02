#pragma once

#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

#include "core/dbus.h"
#include "core/signal.h"

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

struct StatusMessage {
    std::string summary;
    std::string body;

    bool operator==(const StatusMessage &) const = default;
};

std::vector<StatusMessage> network_changes(const NetworkStatus &prev, const NetworkStatus &next);

class NetworkService {
  public:
    explicit NetworkService(SystemBus &bus);

    const NetworkStatus &status() const { return status_; }

    Signal<> changed;
    Signal<const StatusMessage &> messages;

  private:
    bool refresh();

    SystemBus &bus_;
    NetworkStatus status_;
    std::unique_ptr<sdbus::IProxy> manager_;
    sdbus::Slot match_;
};

} // namespace astralia
