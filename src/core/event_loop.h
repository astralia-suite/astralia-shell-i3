#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <poll.h>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>

#include "core/unique_fd.h"
#include "core/x_connection.h"

namespace astralia {

class EventLoop {
  public:
    using EventHandler = std::function<void(const xcb_generic_event_t &)>;
    using NextFire = std::function<std::chrono::milliseconds()>;
    using TimerCallback = std::function<void()>;
    using FdHandler = std::function<void()>;
    using PollPrepare = std::function<int(std::vector<pollfd> &)>;
    using PollDispatch = std::function<void(std::span<const pollfd>)>;

    static std::expected<EventLoop, std::string> create(XConnection &x);

    void on_window(xcb_window_t window, EventHandler handler);
    void on_event(uint8_t type, EventHandler handler);
    int add_timer(NextFire next_fire, TimerCallback callback);
    void reschedule(int timer);
    void on_fd(int fd, FdHandler handler);
    void remove_fd(int fd);
    int add_poll_source(PollPrepare prepare, PollDispatch dispatch);
    void remove_poll_source(int id);
    void stop(int exit_code);
    bool stopping() const { return exit_code_.has_value(); }
    int run();

  private:
    struct Timer {
        NextFire next_fire;
        TimerCallback callback;
        std::chrono::nanoseconds deadline;
    };

    struct PollSource {
        int id;
        PollPrepare prepare;
        PollDispatch dispatch;
    };

    struct PolledRange {
        int id;
        std::size_t start;
        std::size_t count;
    };

    EventLoop(XConnection &x, UniqueFd signal_fd, UniqueFd timer_fd);

    void dispatch(const xcb_generic_event_t &event);
    void read_signal();
    void fire_timers();
    void arm_timer();

    XConnection *x_;
    UniqueFd signal_fd_;
    UniqueFd timer_fd_;
    std::unordered_map<xcb_window_t, EventHandler> handlers_;
    std::unordered_map<uint8_t, EventHandler> type_handlers_;
    std::vector<Timer> timers_;
    std::unordered_map<int, FdHandler> fd_handlers_;
    std::vector<PollSource> poll_sources_;
    int next_poll_source_ = 0;
    std::optional<int> exit_code_;
};

} // namespace astralia
