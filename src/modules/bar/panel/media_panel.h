#pragma once

#include <string>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/image_decode.h"
#include "render/panel_chrome.h"
#include "render/panel_window.h"

#include "service/media_service.h"

namespace astralia {

class MediaPanel {
  public:
    MediaPanel(XConnection &x, EventLoop &loop, MediaService &media);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { close_panel = 1,
                  previous,
                  play_pause,
                  next };

    void handle(const xcb_generic_event_t &event);
    void paint();
    void draw_art(cairo_t *cr, double x, double y);

    XConnection &x_;
    EventLoop &loop_;
    MediaService &media_;
    PanelWindow window_;
    std::vector<PanelHit> hits_;
    std::string art_path_;
    SurfacePtr art_;
    int poll_timer_ = -1;
};

} // namespace astralia
