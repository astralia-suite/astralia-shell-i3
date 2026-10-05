#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/panel_chrome.h"
#include "render/panel_window.h"

#include "service/audio_service.h"

namespace astralia {

class VolumePanel {
  public:
    VolumePanel(XConnection &x, EventLoop &loop, AudioService &audio);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { close_panel = 1,
                  slider,
                  mute,
                  device };

    struct Drag {
        uint32_t id;
        PanelRect track;
    };

    void handle(const xcb_generic_event_t &event);
    void hover(uint32_t id);
    void press(int x, int y, xcb_button_t button);
    void step(uint32_t id, int delta);
    void set_percent(uint32_t id, int percent);
    void paint();

    AudioService &audio_;
    PanelWindow window_;
    std::vector<PanelHit> hits_;
    std::optional<Drag> dragging_;
    uint32_t selected_ = 0;
    uint32_t hovered_ = 0;
    int scroll_ = 0;
    int content_height_ = 0;
    int visible_height_ = 0;
};

} // namespace astralia
