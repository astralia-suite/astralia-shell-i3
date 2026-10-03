#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/panel_chrome.h"
#include "render/panel_window.h"
#include "render/text.h"

namespace astralia {

class ControlCenterPanel {
  public:
    ControlCenterPanel(XConnection &x, EventLoop &loop, Services &services);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Row : std::size_t { brightness,
                             volume };

    struct Slider {
        Text icon;
        Text label;
        int percent = 0;
        bool muted = false;
        bool enabled = false;

        Slider();
    };

    void open();
    void sync_brightness();
    void sync_volume();
    void handle(const xcb_generic_event_t &event);
    void hover(std::optional<Row> row);
    void press(int x, int y, xcb_button_t button);
    void apply(Row row, int percent);
    std::optional<Row> row_at(int y) const;
    int track_x() const;
    int track_width() const;
    void paint();

    BrightnessService &brightness_;
    AudioService &audio_;
    std::array<Slider, 2> sliders_;
    Text percent_sample_;
    std::optional<Row> dragging_;
    std::optional<Row> hovered_;
    std::vector<PanelHit> hits_;
    PanelWindow window_;
};

} // namespace astralia
