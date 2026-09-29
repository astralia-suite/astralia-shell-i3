#pragma once

#include <cairo.h>
#include <optional>
#include <xcb/xcb.h>

#include "core/app_fonts.h"
#include "core/event_loop.h"
#include "core/image_decode.h"
#include "core/ipc.h"
#include "core/keyboard.h"
#include "core/text.h"
#include "core/x_connection.h"

namespace astralia {

class Logout {
  public:
    Logout(XConnection &x, EventLoop &loop, IpcServer &ipc);
    ~Logout();
    Logout(const Logout &) = delete;
    Logout &operator=(const Logout &) = delete;

  private:
    void toggle();
    void open();
    void close();
    void place(const OutputGeometry &output);
    OutputGeometry pointer_output() const;
    void take_focus();
    void restore_focus();
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void click(double x, double y);
    void hover(std::optional<int> button);
    void execute(int index);
    std::optional<int> button_at(double x, double y) const;
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
    Keyboard keyboard_;
    bool fonts_ = register_app_fonts();
    Text glyph_;
    SurfacePtr logo_;
    bool open_ = false;
    int selected_ = 0;
    int hovered_ = -1;
};

} // namespace astralia
