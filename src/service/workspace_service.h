#pragma once

#include <cstdint>
#include <functional>

#include "core/event_loop.h"
#include "core/x_connection.h"

namespace astralia {

struct WorkspaceStatus {
    uint32_t count = 0;
    uint32_t current = 0;

    bool operator==(const WorkspaceStatus &) const = default;
};

class WorkspaceService {
  public:
    WorkspaceService(XConnection &x, EventLoop &loop, std::function<void()> on_change);

    const WorkspaceStatus &status() const { return status_; }
    void switch_to(uint32_t index);

  private:
    bool refresh();

    XConnection &x_;
    std::function<void()> on_change_;
    WorkspaceStatus status_;
};

} // namespace astralia
