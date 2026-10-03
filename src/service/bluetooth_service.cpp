#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "core/log.h"

#include "service/bluetooth_service.h"

namespace astralia {

namespace {

using Properties = std::map<std::string, sdbus::Variant>;
using ManagedObjects = std::map<sdbus::ObjectPath, std::map<std::string, Properties>>;

const std::string bluez = "org.bluez";
const std::string adapter_interface = "org.bluez.Adapter1";
const std::string device_interface = "org.bluez.Device1";
const std::string battery_interface = "org.bluez.Battery1";

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

BluetoothService::BluetoothService(SystemBus &bus)
    : bus_(bus), root_(bus_.proxy(bluez, "/")) {
    refresh();
    match_ = bus_.add_match("type='signal',sender='org.bluez'", [this] {
        BluetoothStatus prev = status_;
        if (refresh()) {
            std::erase_if(proxies_, [this](const auto &entry) {
                return entry.first != adapter_path_ &&
                       std::ranges::none_of(devices_, [&](const BluetoothDevice &d) { return d.path == entry.first; });
            });
            for (const StatusMessage &message : bluetooth_changes(prev, status_)) {
                messages.emit(message);
            }
            changed.emit();
        }
    });
}

bool BluetoothService::refresh() {
    if (!root_) {
        return false;
    }
    BluetoothStatus next;
    std::vector<BluetoothDevice> devices;
    std::string adapter_path;
    try {
        ManagedObjects objects;
        root_->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(objects);
        for (const auto &[path, interfaces] : objects) {
            if (auto it = interfaces.find(adapter_interface); it != interfaces.end() && !next.present) {
                next.present = true;
                next.powered = flag(it->second, "Powered");
                adapter_path = path;
            }
            auto it = interfaces.find(device_interface);
            if (it == interfaces.end()) {
                continue;
            }
            BluetoothDevice device;
            device.path = path;
            device.name = text(it->second, "Alias");
            if (device.name.empty()) {
                device.name = text(it->second, "Address");
            }
            device.paired = flag(it->second, "Paired");
            device.trusted = flag(it->second, "Trusted");
            device.connected = flag(it->second, "Connected");
            device.connecting = busy_.contains(device.path);
            if (auto battery = interfaces.find(battery_interface);
                device.connected && battery != interfaces.end()) {
                auto percent = battery->second.find("Percentage");
                if (percent != battery->second.end() && percent->second.containsValueOfType<uint8_t>()) {
                    device.battery = percent->second.get<uint8_t>();
                }
            }
            if (device.connected && !next.connected) {
                next.connected = true;
                next.device = device.name;
            }
            devices.push_back(std::move(device));
        }
    } catch (const sdbus::Error &) {
        next = {};
        devices.clear();
    }
    adapter_path_ = adapter_path;
    if (next == status_ && devices == devices_) {
        return false;
    }
    status_ = next;
    devices_ = std::move(devices);
    return true;
}

sdbus::IProxy *BluetoothService::proxy(const std::string &path) {
    if (path.empty()) {
        return nullptr;
    }
    auto &slot = proxies_[path];
    if (!slot) {
        slot = bus_.proxy(bluez, path);
    }
    return slot.get();
}

void BluetoothService::call_device(const std::string &path, const char *method, bool busy) {
    sdbus::IProxy *device = proxy(path);
    if (device == nullptr) {
        return;
    }
    if (busy) {
        busy_.insert(path);
    }
    try {
        device->callMethodAsync(method).onInterface(device_interface).uponReplyInvoke([this, path, method](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("bluetooth: {} failed: {}", method, error->getMessage());
            }
            busy_.erase(path);
            if (refresh()) {
                changed.emit();
            }
        });
    } catch (const sdbus::Error &error) {
        busy_.erase(path);
        log::error("bluetooth: {} failed: {}", method, error.what());
    }
    if (busy && refresh()) {
        changed.emit();
    }
}

void BluetoothService::call_adapter(const char *method) {
    sdbus::IProxy *adapter = proxy(adapter_path_);
    if (adapter == nullptr) {
        return;
    }
    try {
        adapter->callMethodAsync(method).onInterface(adapter_interface).uponReplyInvoke([method](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("bluetooth: {} failed: {}", method, error->getMessage());
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("bluetooth: {} failed: {}", method, error.what());
    }
}

void BluetoothService::set_powered(bool powered) {
    sdbus::IProxy *adapter = proxy(adapter_path_);
    if (adapter == nullptr) {
        return;
    }
    try {
        adapter->setPropertyAsync("Powered").onInterface(adapter_interface).toValue(powered).uponReplyInvoke([](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("bluetooth: cannot set Powered: {}", error->getMessage());
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("bluetooth: cannot set Powered: {}", error.what());
    }
}

void BluetoothService::start_discovery() {
    if (status_.powered) {
        call_adapter("StartDiscovery");
    }
}

void BluetoothService::stop_discovery() {
    if (status_.powered) {
        call_adapter("StopDiscovery");
    }
}

void BluetoothService::connect(const std::string &path) { call_device(path, "Connect", true); }

void BluetoothService::disconnect(const std::string &path) { call_device(path, "Disconnect", false); }

void BluetoothService::pair(const std::string &path) { call_device(path, "Pair", true); }

void BluetoothService::forget(const std::string &path) {
    sdbus::IProxy *adapter = proxy(adapter_path_);
    if (adapter == nullptr) {
        return;
    }
    try {
        adapter->callMethodAsync("RemoveDevice").onInterface(adapter_interface).withArguments(sdbus::ObjectPath(path)).uponReplyInvoke([](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("bluetooth: RemoveDevice failed: {}", error->getMessage());
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("bluetooth: RemoveDevice failed: {}", error.what());
    }
}

} // namespace astralia
