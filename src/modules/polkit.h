#pragma once

#include <cairo.h>
#include <string>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/keyboard.h"
#include "core/x_connection.h"

#include "render/app_fonts.h"
#include "render/image_decode.h"
#include "render/text.h"
#include "render/x_window.h"

namespace astralia {

class Polkit {
  public:
    Polkit(XConnection &x, EventLoop &loop, Services &services);
    ~Polkit();

  private:
    void sync();
    void open();
    void close();
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void clear_password();
    void paint();

    XConnection &x_;
    XWindow window_;
    Keyboard keyboard_;
    bool fonts_ = register_app_fonts();
    Text title_;
    Text message_;
    Text field_;
    Text info_;
    SurfacePtr echo_;
    std::string password_;
    bool error_shown_ = false;
    bool last_error_ = false;
    bool open_ = false;
    PolkitService &service_;
};

} // namespace astralia
