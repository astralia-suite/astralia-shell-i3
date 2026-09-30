#include <utility>

#include "core/dbus.h"
#include "core/log.h"

namespace astralia {

SystemBus::SystemBus(EventLoop &loop, BusKind kind) : loop_(loop) {
    try {
        conn_ = kind == BusKind::session ? sdbus::createSessionBusConnection()
                                         : sdbus::createSystemBusConnection();
    } catch (const sdbus::Error &error) {
        log::error("cannot connect to the {} bus: {}",
                   kind == BusKind::session ? "session" : "system", error.what());
        return;
    }
    sdbus::IConnection::PollData data = conn_->getEventLoopPollData();
    fd_ = data.fd;
    event_fd_ = data.eventFd;
    loop_.on_fd(fd_, [this] { drain(); });
    loop_.on_fd(event_fd_, [this] { drain(); });
}

SystemBus::~SystemBus() {
    if (conn_) {
        loop_.remove_fd(fd_);
        loop_.remove_fd(event_fd_);
    }
}

sdbus::Slot SystemBus::add_match(const std::string &rule, std::function<void()> handler) {
    if (!conn_) {
        return {};
    }
    try {
        return conn_->addMatch(
            rule, [handler = std::move(handler)](sdbus::Message) { handler(); },
            sdbus::return_slot);
    } catch (const sdbus::Error &error) {
        log::error("cannot add D-Bus match {}: {}", rule, error.what());
        return {};
    }
}

std::unique_ptr<sdbus::IProxy> SystemBus::proxy(const std::string &service,
                                                const std::string &path) {
    if (!conn_) {
        return nullptr;
    }
    try {
        return sdbus::createProxy(*conn_, sdbus::ServiceName(service), sdbus::ObjectPath(path));
    } catch (const sdbus::Error &error) {
        log::error("cannot create D-Bus proxy {} {}: {}", service, path, error.what());
        return nullptr;
    }
}

void SystemBus::drain() {
    try {
        while (conn_->processPendingEvent()) {
        }
    } catch (const sdbus::Error &error) {
        log::error("D-Bus processing failed: {}", error.what());
    }
}

} // namespace astralia
