#pragma once

#include <map>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <set>
#include <string>
#include <vector>

#include "core/dbus.h"
#include "core/signal.h"

#include "service/network_service.h"

namespace astralia {

struct BluetoothStatus {
    bool present = false;
    bool powered = false;
    bool connected = false;
    std::string device;
    bool operator==(const BluetoothStatus &) const = default;
};

struct BluetoothDevice {
    std::string path;
    std::string name;
    bool paired = false;
    bool trusted = false;
    bool connected = false;
    bool connecting = false;
    int battery = -1;
    bool operator==(const BluetoothDevice &) const = default;
};

std::vector<StatusMessage> bluetooth_changes(const BluetoothStatus &prev, const BluetoothStatus &next);

class BluetoothService {
  public:
    explicit BluetoothService(SystemBus &bus);
    const BluetoothStatus &status() const { return status_; }
    const std::vector<BluetoothDevice> &devices() const { return devices_; }

    void set_powered(bool powered);
    void start_discovery();
    void stop_discovery();
    void connect(const std::string &path);
    void disconnect(const std::string &path);
    void pair(const std::string &path);
    void forget(const std::string &path);

    Signal<> changed;
    Signal<const StatusMessage &> messages;

  private:
    bool refresh();
    sdbus::IProxy *proxy(const std::string &path);
    void call_device(const std::string &path, const char *method, bool busy);
    void call_adapter(const char *method);

    SystemBus &bus_;
    BluetoothStatus status_;
    std::vector<BluetoothDevice> devices_;
    std::set<std::string> busy_;
    std::string adapter_path_;
    std::unique_ptr<sdbus::IProxy> root_;
    std::map<std::string, std::unique_ptr<sdbus::IProxy>> proxies_;
    sdbus::Slot match_;
};

} // namespace astralia
