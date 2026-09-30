#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "core/event_loop.h"
#include "core/x_connection.h"

namespace astralia {

struct WorkspaceStatus {
    uint32_t count = 0;
    uint32_t current = 0;
    uint32_t occupied = 0;

    bool operator==(const WorkspaceStatus &) const = default;
};

class WorkspaceService {
  public:
    WorkspaceService(XConnection &x, EventLoop &loop, std::function<void()> on_change);

    const WorkspaceStatus &status() const { return status_; }
    void switch_to(uint32_t index);

  private:
    bool refresh();
    std::string i3_socket_path();
    void i3_command(std::string_view command);

    XConnection &x_;
    std::function<void()> on_change_;
    WorkspaceStatus status_;
};

} // namespace astralia
