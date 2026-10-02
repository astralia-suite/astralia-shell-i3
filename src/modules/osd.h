#pragma once

#include <cairo.h>
#include <chrono>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/app_fonts.h"
#include "core/event_loop.h"
#include "core/text.h"
#include "core/x_connection.h"

namespace astralia {

class Osd {
  public:
    Osd(XConnection &x, EventLoop &loop, Services &services);
    ~Osd();
    Osd(const Osd &) = delete;
    Osd &operator=(const Osd &) = delete;

  private:
    enum class Kind { brightness,
                      volume,
                      mic };

    void show(Kind kind, int percent, bool muted);
    void hide();
    std::chrono::milliseconds until_hide() const;
    OutputGeometry pointer_output() const;
    void paint(Kind kind, int percent, bool muted);
    void present();

    XConnection &x_;
    EventLoop &loop_;
    Services &services_;
    xcb_colormap_t colormap_;
    xcb_window_t window_;
    xcb_gcontext_t gc_;
    xcb_pixmap_t pixmap_;
    cairo_surface_t *surface_;
    cairo_t *cr_;
    bool fonts_ = register_app_fonts();
    Text icon_;
    Text label_;
    int timer_;
    bool mapped_ = false;
    std::chrono::steady_clock::time_point ready_at_;
    std::chrono::steady_clock::time_point hide_at_;
};

} // namespace astralia
