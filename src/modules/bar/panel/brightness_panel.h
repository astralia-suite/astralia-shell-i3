#pragma once

#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/panel_chrome.h"
#include "render/panel_window.h"
#include "render/text.h"

#include "service/brightness_service.h"

namespace astralia {

class BrightnessPanel {
  public:
    BrightnessPanel(XConnection &x, EventLoop &loop, BrightnessService &brightness);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    void open();
    void sync();
    void handle(const xcb_generic_event_t &event);
    void hover(bool hovered);
    void press(int x, int y, xcb_button_t button);
    void apply(int percent);
    bool on_row(int y) const;
    int track_x() const;
    int track_width() const;
    void paint();

    BrightnessService &brightness_;
    Text icon_;
    Text label_;
    Text percent_sample_;
    int percent_ = 0;
    bool enabled_ = false;
    bool dragging_ = false;
    bool hovered_ = false;
    std::vector<PanelHit> hits_;
    PanelWindow window_;
};

} // namespace astralia
