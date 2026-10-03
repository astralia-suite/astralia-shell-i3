#pragma once

#include <cairo.h>
#include <optional>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/keyboard.h"
#include "core/x_connection.h"

#include "render/app_fonts.h"
#include "render/image_decode.h"
#include "render/text.h"
#include "render/x_window.h"

namespace astralia {

class Logout {
  public:
    Logout(XConnection &x, EventLoop &loop, IpcServer &ipc);

  private:
    void toggle();
    void open();
    void close();
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void click(double x, double y);
    void hover(std::optional<int> button);
    void execute(int index);
    std::optional<int> button_at(double x, double y) const;
    void paint();

    XConnection &x_;
    XWindow window_;
    Keyboard keyboard_;
    bool fonts_ = register_app_fonts();
    Text glyph_;
    SurfacePtr logo_;
    bool open_ = false;
    int selected_ = 0;
    int hovered_ = -1;
};

} // namespace astralia
