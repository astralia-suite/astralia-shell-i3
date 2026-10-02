#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

#include "core/dbus.h"
#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

struct Notification {
    uint32_t id;
    std::string app;
    std::string summary;
    std::string body;
    bool critical;
    std::chrono::steady_clock::time_point deadline;
};

class NotificationService {
  public:
    explicit NotificationService(EventLoop &loop);
    NotificationService(const NotificationService &) = delete;
    NotificationService &operator=(const NotificationService &) = delete;

    const std::vector<Notification> &list() const { return list_; }
    void dismiss(uint32_t id);

    Signal<> changed;

  private:
    uint32_t notify(std::string app, uint32_t replaces_id, std::string summary, std::string body,
                    const std::map<std::string, sdbus::Variant> &hints);
    void close(uint32_t id, uint32_t reason);
    void expire();
    std::chrono::milliseconds until_next() const;

    EventLoop &loop_;
    SystemBus bus_;
    std::unique_ptr<sdbus::IObject> object_;
    std::vector<Notification> list_;
    uint32_t next_id_ = 1;
    int timer_ = -1;
};

} // namespace astralia
