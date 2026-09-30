#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "service/network_service.h"

namespace astralia {

namespace {

const std::string nm_service = "org.freedesktop.NetworkManager";
const std::string nm_path = "/org/freedesktop/NetworkManager";
constexpr uint32_t connectivity_portal = 2;

std::string wifi_ssid(const NetworkStatus &status) {
    return status.kind == NetworkKind::wifi ? status.ssid : std::string();
}

} // namespace

std::vector<StatusMessage> network_changes(const NetworkStatus &prev, const NetworkStatus &next) {
    std::vector<StatusMessage> messages;
    std::string was = wifi_ssid(prev);
    std::string now = wifi_ssid(next);
    if (now != was) {
        if (!now.empty()) {
            messages.push_back({"Connected", "Connected to " + now});
        } else {
            messages.push_back({"Disconnected", "Disconnected from " + was});
        }
    }
    bool was_ethernet = prev.kind == NetworkKind::ethernet;
    bool now_ethernet = next.kind == NetworkKind::ethernet;
    if (now_ethernet && !was_ethernet) {
        messages.push_back({"Connected", "Connected via Ethernet"});
    } else if (was_ethernet && !now_ethernet) {
        messages.push_back({"Disconnected", "Ethernet disconnected"});
    }
    if (next.portal && !prev.portal) {
        messages.push_back({"Captive Portal", now.empty() ? std::string("Sign in required")
                                                          : "Sign in required for " + now});
    }
    return messages;
}

NetworkService::NetworkService(SystemBus &bus, std::function<void()> on_change, NotifyFn notify)
    : bus_(bus), on_change_(std::move(on_change)), notify_(std::move(notify)),
      manager_(bus_.proxy(nm_service, nm_path)) {
    refresh();
    match_ = bus_.add_match("type='signal',sender='org.freedesktop.NetworkManager',"
                            "interface='org.freedesktop.DBus.Properties',"
                            "member='PropertiesChanged'",
                            [this] {
                                NetworkStatus prev = status_;
                                if (refresh()) {
                                    for (const StatusMessage &message : network_changes(prev, status_)) {
                                        notify_(message);
                                    }
                                    on_change_();
                                }
                            });
}

bool NetworkService::refresh() {
    sdbus::IConnection *conn = bus_.conn();
    if (conn == nullptr || !manager_) {
        return false;
    }
    NetworkStatus next;
    std::string type =
        dbus_property<std::string>(manager_.get(), nm_service, "PrimaryConnectionType")
            .value_or("");
    next.portal = dbus_property<uint32_t>(manager_.get(), nm_service, "Connectivity") ==
                  connectivity_portal;
    if (type == "802-3-ethernet") {
        next.kind = NetworkKind::ethernet;
    } else if (type == "802-11-wireless") {
        next.kind = NetworkKind::wifi;
        auto active =
            dbus_property<sdbus::ObjectPath>(manager_.get(), nm_service, "PrimaryConnection");
        auto access_point = active ? dbus_property<sdbus::ObjectPath>(
                                         *conn, nm_service, *active,
                                         nm_service + ".Connection.Active", "SpecificObject")
                                   : std::nullopt;
        if (access_point) {
            const std::string interface = nm_service + ".AccessPoint";
            next.strength =
                dbus_property<uint8_t>(*conn, nm_service, *access_point, interface, "Strength")
                    .value_or(0);
            auto ssid = dbus_property<std::vector<uint8_t>>(*conn, nm_service, *access_point,
                                                            interface, "Ssid");
            if (ssid) {
                next.ssid.assign(ssid->begin(), ssid->end());
            }
        }
    }
    if (next == status_) {
        return false;
    }
    status_ = std::move(next);
    return true;
}

} // namespace astralia
