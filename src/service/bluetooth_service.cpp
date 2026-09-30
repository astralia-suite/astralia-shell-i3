#include <map>
#include <string>
#include <utility>
#include <vector>

#include "service/bluetooth_service.h"

namespace astralia {

namespace {

using Properties = std::map<std::string, sdbus::Variant>;
using ManagedObjects = std::map<sdbus::ObjectPath, std::map<std::string, Properties>>;

bool flag(const Properties &properties, const std::string &name) {
    auto it = properties.find(name);
    return it != properties.end() && it->second.containsValueOfType<bool>() &&
           it->second.get<bool>();
}

std::string text(const Properties &properties, const std::string &name) {
    auto it = properties.find(name);
    if (it == properties.end() || !it->second.containsValueOfType<std::string>()) {
        return {};
    }
    return it->second.get<std::string>();
}

std::string connected_device(const BluetoothStatus &status) {
    return status.connected ? status.device : std::string();
}

} // namespace

std::vector<StatusMessage> bluetooth_changes(const BluetoothStatus &prev, const BluetoothStatus &next) {
    std::string was = connected_device(prev);
    std::string now = connected_device(next);
    if (now == was && prev.connected == next.connected) {
        return {};
    }
    if (next.connected) {
        return {{"Connected", "Connected to " + now}};
    }
    return {{"Disconnected", "Disconnected from " + was}};
}

BluetoothService::BluetoothService(SystemBus &bus, std::function<void()> on_change, NotifyFn notify)
    : bus_(bus), on_change_(std::move(on_change)), notify_(std::move(notify)),
      root_(bus_.proxy("org.bluez", "/")) {
    refresh();
    match_ = bus_.add_match("type='signal',sender='org.bluez'", [this] {
        BluetoothStatus prev = status_;
        if (refresh()) {
            for (const StatusMessage &message : bluetooth_changes(prev, status_)) {
                notify_(message);
            }
            on_change_();
        }
    });
}

bool BluetoothService::refresh() {
    if (!root_) {
        return false;
    }
    BluetoothStatus next;
    try {
        ManagedObjects objects;
        root_->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(objects);
        for (const auto &[path, interfaces] : objects) {
            if (auto it = interfaces.find("org.bluez.Adapter1"); it != interfaces.end()) {
                next.present = true;
                next.powered = next.powered || flag(it->second, "Powered");
            }
            if (auto it = interfaces.find("org.bluez.Device1");
                it != interfaces.end() && !next.connected && flag(it->second, "Connected")) {
                next.connected = true;
                next.device = text(it->second, "Alias");
            }
        }
    } catch (const sdbus::Error &) {
        next = {};
    }
    if (next == status_) {
        return false;
    }
    status_ = next;
    return true;
}

} // namespace astralia
