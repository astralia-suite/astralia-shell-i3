#pragma once

#include <string>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/unique_fd.h"
#include "core/x_connection.h"

namespace astralia {

class Wallpaper {
  public:
    Wallpaper(XConnection &x, EventLoop &loop);
    ~Wallpaper();
    Wallpaper(const Wallpaper &) = delete;
    Wallpaper &operator=(const Wallpaper &) = delete;

  private:
    void watch_config(EventLoop &loop);
    bool config_changed();
    void apply(bool force);

    XConnection &x_;
    std::string config_path_;
    std::string config_name_;
    UniqueFd inotify_;
    std::vector<Output> applied_;
    xcb_pixmap_t pixmap_ = XCB_NONE;
};

} // namespace astralia
