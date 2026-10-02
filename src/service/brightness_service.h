#pragma once

#include <string>

#include "core/event_loop.h"
#include "core/signal.h"
#include "core/unique_fd.h"

namespace astralia {

class BrightnessService {
  public:
    explicit BrightnessService(EventLoop &loop);
    ~BrightnessService();
    BrightnessService(const BrightnessService &) = delete;
    BrightnessService &operator=(const BrightnessService &) = delete;

    bool available() const { return max_ > 0; }
    int percent() const;
    void set(int percent) const;

    Signal<> changed;

  private:
    EventLoop &loop_;
    std::string device_;
    int max_ = 0;
    UniqueFd watch_;
};

} // namespace astralia
