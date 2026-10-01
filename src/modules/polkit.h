#pragma once

#include <cairo.h>
#include <string>
#include <xcb/xcb.h>

#include "core/app_fonts.h"
#include "core/event_loop.h"
#include "core/image_decode.h"
#include "core/keyboard.h"
#include "core/text.h"
#include "core/x_connection.h"

#include "service/polkit_service.h"

namespace astralia {

class Polkit {
  public:
    Polkit(XConnection &x, EventLoop &loop);
    ~Polkit();
    Polkit(const Polkit &) = delete;
    Polkit &operator=(const Polkit &) = delete;

  private:
    void sync();
    void open();
    void close();
    void place(const OutputGeometry &output);
    OutputGeometry pointer_output() const;
    void take_focus();
    void restore_focus();
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void clear_password();
    void paint();
    void present();

    XConnection &x_;
    xcb_visualtype_t *visual_;
    uint8_t depth_;
    xcb_colormap_t colormap_;
    xcb_window_t window_;
    xcb_window_t previous_focus_ = XCB_NONE;
    xcb_gcontext_t gc_;
    xcb_pixmap_t pixmap_ = XCB_NONE;
    cairo_surface_t *surface_ = nullptr;
    cairo_t *cr_ = nullptr;
    OutputGeometry geometry_{};
    double scale_ = 1.0;
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
    PolkitService service_;
};

} // namespace astralia
