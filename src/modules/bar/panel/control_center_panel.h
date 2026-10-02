#pragma once

#include <array>
#include <cairo.h>
#include <cstddef>
#include <optional>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/keyboard.h"
#include "core/text.h"
#include "core/x_connection.h"

namespace astralia {

int slider_percent_at(int track_x, int track_width, int px);

class ControlCenterPanel {
  public:
    ControlCenterPanel(XConnection &x, EventLoop &loop, Services &services);
    ~ControlCenterPanel();
    ControlCenterPanel(const ControlCenterPanel &) = delete;
    ControlCenterPanel &operator=(const ControlCenterPanel &) = delete;

    void toggle();

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
    void close();
    void take_focus();
    void restore_focus();
    void handle(const xcb_generic_event_t &event);
    void press(int x, int y, xcb_button_t button);
    void apply(Row row, int percent);
    std::optional<Row> row_at(int y) const;
    int track_x() const;
    int track_width() const;
    void paint();
    void present();

    XConnection &x_;
    Keyboard keyboard_;
    BrightnessService &brightness_;
    AudioService &audio_;
    std::array<Slider, 2> sliders_;
    Text percent_sample_;
    std::optional<Row> dragging_;
    bool open_ = false;
    int width_;
    int height_;
    xcb_colormap_t colormap_;
    xcb_window_t window_;
    xcb_window_t previous_focus_ = XCB_NONE;
    xcb_pixmap_t pixmap_;
    xcb_gcontext_t gc_;
    cairo_surface_t *surface_;
    cairo_t *cr_;
};

} // namespace astralia
