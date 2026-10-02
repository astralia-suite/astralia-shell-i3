#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "core/event_loop.h"
#include "core/signal.h"
#include "core/x_connection.h"

namespace astralia {

struct I3Status {
    uint32_t count = 0;
    uint32_t current = 0;
    uint32_t occupied = 0;

    bool operator==(const I3Status &) const = default;
};

class I3Service {
  public:
    I3Service(XConnection &x, EventLoop &loop);

    const I3Status &status() const { return status_; }
    void switch_to(uint32_t index);

    Signal<> changed;

  private:
    bool refresh();
    std::string i3_socket_path();
    void i3_command(std::string_view command);

    XConnection &x_;
    I3Status status_;
};

} // namespace astralia
