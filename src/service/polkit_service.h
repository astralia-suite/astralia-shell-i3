#pragma once

#include <memory>
#include <string>

#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

class PolkitService {
  public:
    explicit PolkitService(EventLoop &loop);
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

    Signal<> changed;

  private:
    struct Impl;

    EventLoop &loop_;
    std::unique_ptr<Impl> impl_;
    int source_ = -1;
};

} // namespace astralia
