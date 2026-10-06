#pragma once

#include <cairo.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/keyboard.h"
#include "core/x_connection.h"

#include "modules/overview/layout.h"

#include "render/image_decode.h"
#include "render/text.h"
#include "render/x_window.h"

namespace astralia {

class Overview {
  public:
    Overview(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services);

  private:
    struct Tile {
        const I3Window *window;
        OverviewRect rect;
    };

    void toggle();
    void open();
    void close();
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void press(double x, double y);
    void move(double x, double y);
    void release();
    void select(uint32_t workspace);
    void refresh_tree();
    void expect_focus_loss();
    void compute_layout();
    std::vector<Tile> tiles() const;
    const Tile *tile_at(const std::vector<Tile> &tiles, double x, double y) const;
    cairo_surface_t *icon_for(const std::string &window_class, int size);
    void paint();

    XConnection &x_;
    Services &services_;
    XWindow window_;
    Keyboard keyboard_;
    Text number_;
    bool open_ = false;
    uint32_t selected_ = 1;
    uint32_t page_ = 0;
    I3Tree tree_;
    OverviewLayout layout_;
    double work_width_ = 1.0;
    double work_height_ = 1.0;
    std::chrono::steady_clock::time_point focus_grace_until_{};
    std::unordered_map<std::string, SurfacePtr> icons_;

    bool dragging_ = false;
    int64_t drag_id_ = 0;
    uint32_t drag_from_ = 0;
    double drag_dx_ = 0.0;
    double drag_dy_ = 0.0;
    double pointer_x_ = 0.0;
    double pointer_y_ = 0.0;
};

} // namespace astralia
