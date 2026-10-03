#pragma once

#include <cairo.h>
#include <chrono>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/app_fonts.h"
#include "render/text.h"
#include "render/x_window.h"

namespace astralia {

class Osd {
  public:
    Osd(XConnection &x, EventLoop &loop, Services &services);

  private:
    enum class Kind { brightness,
                      volume,
                      mic };

    void show(Kind kind, int percent, bool muted);
    void hide();
    std::chrono::milliseconds until_hide() const;
    void paint(Kind kind, int percent, bool muted);

    XConnection &x_;
    EventLoop &loop_;
    Services &services_;
    XWindow window_;
    bool fonts_ = register_app_fonts();
    Text icon_;
    Text label_;
    int timer_;
    std::chrono::steady_clock::time_point ready_at_;
    std::chrono::steady_clock::time_point hide_at_;
};

} // namespace astralia
