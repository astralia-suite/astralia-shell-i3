#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/event_loop.h"
#include "core/json.h"
#include "core/signal.h"
#include "core/x_connection.h"

namespace astralia {

struct I3Status {
    uint32_t count = 0;
    uint32_t current = 0;
    uint32_t occupied = 0;

    bool operator==(const I3Status &) const = default;
};

struct I3Window {
    int64_t id = 0;
    std::string window_class;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    uint32_t workspace = 0;
    bool floating = false;
    bool fullscreen = false;

    bool operator==(const I3Window &) const = default;
};

struct I3Workspace {
    uint32_t number = 0;
    double width = 0.0;
    double height = 0.0;

    bool operator==(const I3Workspace &) const = default;
};

struct I3Tree {
    std::vector<I3Workspace> workspaces;
    std::vector<I3Window> windows;
};

I3Tree parse_i3_tree(const Json &root);

class I3Service {
  public:
    I3Service(XConnection &x, EventLoop &loop);

    const I3Status &status() const { return status_; }
    void switch_to(uint32_t index);
    I3Tree query_tree();
    void move_window(int64_t id, uint32_t workspace);
    void kill_window(int64_t id);

    Signal<> changed;
    Signal<> windows_changed;

  private:
    bool refresh();
    std::string i3_socket_path();
    std::optional<std::string> i3_request(uint32_t type, std::string_view payload, bool want_reply);
    void i3_command(std::string_view command);

    XConnection &x_;
    I3Status status_;
};

} // namespace astralia
