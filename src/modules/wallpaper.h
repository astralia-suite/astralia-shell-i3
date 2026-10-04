#pragma once

#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/x_connection.h"

namespace astralia {

class Wallpaper {
  public:
    Wallpaper(XConnection &x, Services &services);
    ~Wallpaper();
    Wallpaper(const Wallpaper &) = delete;
    Wallpaper &operator=(const Wallpaper &) = delete;

  private:
    void apply(bool force);

    XConnection &x_;
    Services &services_;
    std::vector<Output> applied_;
    xcb_pixmap_t pixmap_ = XCB_NONE;
};

} // namespace astralia
