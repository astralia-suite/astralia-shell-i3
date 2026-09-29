#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "core/event_loop.h"

namespace astralia {

class SystemBus {
  public:
    explicit SystemBus(EventLoop &loop);
    ~SystemBus();
    SystemBus(const SystemBus &) = delete;
    SystemBus &operator=(const SystemBus &) = delete;

    sdbus::IConnection *conn() const { return conn_.get(); }
    sdbus::Slot add_match(const std::string &rule, std::function<void()> handler);
    std::unique_ptr<sdbus::IProxy> proxy(const std::string &service, const std::string &path);

  private:
    void drain();

    EventLoop &loop_;
    std::unique_ptr<sdbus::IConnection> conn_;
    int fd_ = -1;
    int event_fd_ = -1;
};

template <typename T>
std::optional<T> dbus_property(sdbus::IProxy *proxy, const std::string &interface,
                               const std::string &name) {
    if (proxy == nullptr) {
        return std::nullopt;
    }
    try {
        return proxy->getProperty(name).onInterface(interface).template get<T>();
    } catch (const sdbus::Error &) {
        return std::nullopt;
    }
}

template <typename T>
std::optional<T> dbus_property(sdbus::IConnection &conn, const std::string &service,
                               const std::string &path, const std::string &interface,
                               const std::string &name) {
    try {
        auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(service), sdbus::ObjectPath(path));
        return dbus_property<T>(proxy.get(), interface, name);
    } catch (const sdbus::Error &) {
        return std::nullopt;
    }
}

} // namespace astralia
