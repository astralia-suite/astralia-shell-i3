#pragma once

#include <cairo.h>
#include <cstdint>
#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/app_fonts.h"
#include "render/text.h"
#include "render/x_window.h"

namespace astralia {

class Notifications {
  public:
    Notifications(XConnection &x, EventLoop &loop, Services &services);

  private:
    void sync();
    void handle(const xcb_generic_event_t &event);
    double measure(const Notification &n);
    void paint();

    XConnection &x_;
    XWindow window_;
    bool fonts_ = register_app_fonts();
    Text app_;
    Text summary_;
    Text body_;
    std::vector<Notification> shown_;
    std::vector<double> heights_;
    NotificationService &service_;
};

} // namespace astralia
