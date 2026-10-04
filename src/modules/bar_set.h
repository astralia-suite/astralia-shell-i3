#pragma once

#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/x_connection.h"

namespace astralia {

class Bar;

class BarSet {
  public:
    BarSet(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services);
    ~BarSet();
    BarSet(const BarSet &) = delete;
    BarSet &operator=(const BarSet &) = delete;

  private:
    struct Entry {
        std::string output;
        std::unique_ptr<Bar> bar;
    };

    void sync();
    void notify(const std::string &app, const StatusMessage &message);

    XConnection &x_;
    EventLoop &loop_;
    IpcServer &ipc_;
    Services &services_;
    std::unique_ptr<sdbus::IProxy> notifier_;
    std::vector<Entry> bars_;
};

} // namespace astralia
