#pragma once

#include <cairo.h>
#include <functional>
#include <string_view>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/keyboard.h"
#include "core/signal.h"
#include "core/x_connection.h"

#include "render/x_window.h"

namespace astralia {

class PanelWindow {
  public:
    using Handler = std::function<void(const xcb_generic_event_t &)>;

    PanelWindow(XConnection &x, EventLoop &loop, std::string_view name, int width, int max_height, Handler handler, std::function<void()> closed);

    bool is_open() const { return open_; }
    void open(int right_margin, int top);
    void close();
    void set_height(int height);
    int width() const { return width_; }
    int height() const { return height_; }
    int max_height() const { return max_height_; }
    cairo_t *cr();
    Keyboard &keyboard() { return keyboard_; }
    void clear();
    void present();

    Signal<> changed;

  private:
    void handle(const xcb_generic_event_t &event);
    void place();
    void grab();

    XConnection &x_;
    Keyboard keyboard_;
    Handler handler_;
    std::function<void()> closed_;
    bool open_ = false;
    bool grabbed_ = false;
    int width_;
    int height_;
    int max_height_;
    OutputGeometry anchor_{0, 0, 1, 1};
    XWindow window_;
};

} // namespace astralia
