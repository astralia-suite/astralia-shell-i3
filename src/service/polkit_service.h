#pragma once

#include <functional>
#include <memory>
#include <string>

#include "core/event_loop.h"

namespace astralia {

class PolkitService {
  public:
    PolkitService(EventLoop &loop, std::function<void()> on_change);
    ~PolkitService();
    PolkitService(const PolkitService &) = delete;
    PolkitService &operator=(const PolkitService &) = delete;

    bool pending() const;
    std::string message() const;
    bool response_required() const;
    std::string info() const;
    bool info_is_error() const;
    void respond(std::string &response);
    void cancel();

  private:
    struct Impl;

    EventLoop &loop_;
    std::unique_ptr<Impl> impl_;
    int source_ = -1;
};

} // namespace astralia
