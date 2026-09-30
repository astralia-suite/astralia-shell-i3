#include <algorithm>
#include <tuple>
#include <utility>

#include "config/notification_config.h"

#include "core/log.h"

#include "service/notification_service.h"

namespace astralia {

namespace {

constexpr const char *bus_name = "org.freedesktop.Notifications";
constexpr const char *object_path = "/org/freedesktop/Notifications";
constexpr const char *interface = "org.freedesktop.Notifications";

// Close reasons
constexpr uint32_t reason_expired = 1;
constexpr uint32_t reason_dismissed = 2;
constexpr uint32_t reason_closed = 3;

// Urgency
constexpr uint8_t urgency_critical = 2;

} // namespace

NotificationService::NotificationService(EventLoop &loop, std::function<void()> on_change)
    : loop_(loop), on_change_(std::move(on_change)), bus_(loop, BusKind::session) {
    timer_ = loop_.add_timer([this] { return until_next(); }, [this] { expire(); });
    sdbus::IConnection *conn = bus_.conn();
    if (conn == nullptr) {
        return;
    }
    try {
        object_ = sdbus::createObject(*conn, sdbus::ObjectPath(object_path));
        object_
            ->addVTable(
                sdbus::registerMethod("Notify").implementedAs(
                    [this](const std::string &app, uint32_t replaces_id, const std::string &,
                           const std::string &summary, const std::string &body,
                           const std::vector<std::string> &,
                           const std::map<std::string, sdbus::Variant> &hints, int32_t) {
                        return notify(app, replaces_id, summary, body, hints);
                    }),
                sdbus::registerMethod("CloseNotification").implementedAs([this](uint32_t id) {
                    close(id, reason_closed);
                }),
                sdbus::registerMethod("GetCapabilities").implementedAs([] {
                    return std::vector<std::string>{"body"};
                }),
                sdbus::registerMethod("GetServerInformation").implementedAs([] {
                    return std::make_tuple(std::string("astralia-shell"), std::string("astralia"),
                                           std::string("0.1.0"), std::string("1.2"));
                }),
                sdbus::registerSignal("NotificationClosed").withParameters<uint32_t, uint32_t>())
            .forInterface(interface);
        conn->requestName(sdbus::ServiceName(bus_name));
    } catch (const sdbus::Error &error) {
        log::error("notification: cannot own {}: {}", bus_name, error.what());
        object_.reset();
    }
}

void NotificationService::dismiss(uint32_t id) { close(id, reason_dismissed); }

uint32_t NotificationService::notify(std::string app, uint32_t replaces_id, std::string summary,
                                     std::string body,
                                     const std::map<std::string, sdbus::Variant> &hints) {
    auto urgency = hints.find("urgency");
    bool critical = urgency != hints.end() && urgency->second.containsValueOfType<uint8_t>() &&
                    urgency->second.get<uint8_t>() == urgency_critical;
    auto replaced = std::ranges::find(list_, replaces_id, &Notification::id);
    uint32_t id = replaces_id;
    if (replaces_id == 0 || replaced == list_.end()) {
        id = next_id_++;
    } else {
        list_.erase(replaced);
    }
    list_.push_back({id, std::move(app), std::move(summary), std::move(body), critical,
                     std::chrono::steady_clock::now() + notification_config::hang_time});
    loop_.reschedule(timer_);
    on_change_();
    return id;
}

void NotificationService::close(uint32_t id, uint32_t reason) {
    auto it = std::ranges::find(list_, id, &Notification::id);
    if (it == list_.end()) {
        return;
    }
    list_.erase(it);
    if (object_) {
        try {
            object_->emitSignal("NotificationClosed").onInterface(interface).withArguments(id, reason);
        } catch (const sdbus::Error &error) {
            log::error("notification: cannot emit NotificationClosed: {}", error.what());
        }
    }
    on_change_();
}

void NotificationService::expire() {
    auto now = std::chrono::steady_clock::now();
    while (!list_.empty() && list_.front().deadline <= now) {
        close(list_.front().id, reason_expired);
    }
}

std::chrono::milliseconds NotificationService::until_next() const {
    if (list_.empty()) {
        return std::chrono::hours(1);
    }
    auto left = list_.front().deadline - std::chrono::steady_clock::now();
    return std::max(std::chrono::ceil<std::chrono::milliseconds>(left), std::chrono::milliseconds(0));
}

} // namespace astralia
