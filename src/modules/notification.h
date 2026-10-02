#pragma once

#include <cairo.h>
#include <cstdint>
#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/app_fonts.h"
#include "core/event_loop.h"
#include "core/text.h"
#include "core/x_connection.h"

namespace astralia {

class Notifications {
  public:
    Notifications(XConnection &x, EventLoop &loop, Services &services);
    ~Notifications();
    Notifications(const Notifications &) = delete;
    Notifications &operator=(const Notifications &) = delete;

  private:
    void sync();
    void place(int x, int y, uint16_t width, uint16_t height);
    OutputGeometry pointer_output() const;
    void handle(const xcb_generic_event_t &event);
    double measure(const Notification &n);
    void paint();
    void present();

    XConnection &x_;
    xcb_visualtype_t *visual_;
    uint8_t depth_;
    xcb_colormap_t colormap_;
    xcb_window_t window_;
    xcb_gcontext_t gc_;
    xcb_pixmap_t pixmap_ = XCB_NONE;
    cairo_surface_t *surface_ = nullptr;
    cairo_t *cr_ = nullptr;
    OutputGeometry geometry_{};
    bool fonts_ = register_app_fonts();
    Text app_;
    Text summary_;
    Text body_;
    std::vector<Notification> shown_;
    std::vector<double> heights_;
    bool mapped_ = false;
    NotificationService &service_;
};

} // namespace astralia
