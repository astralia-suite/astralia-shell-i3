#pragma once

#include <cairo.h>
#include <cstdint>
#include <string_view>
#include <xcb/xcb.h>

#include "core/x_connection.h"

namespace astralia {

class XWindow {
  public:
    XWindow(XConnection &x, std::string_view name, uint32_t event_mask, bool override_redirect = true);
    ~XWindow();
    XWindow(const XWindow &) = delete;
    XWindow &operator=(const XWindow &) = delete;

    xcb_window_t id() const { return window_; }
    cairo_t *cr() const { return cr_; }
    const OutputGeometry &geometry() const { return geometry_; }
    bool mapped() const { return mapped_; }

    void place(const OutputGeometry &geometry, uint16_t backing_width = 0, uint16_t backing_height = 0);
    void release();
    void show(bool focus);
    void focus();
    void hide();
    void clear();
    void present();
    void present(int x, int y, int width, int height);

  private:
    void take_focus();
    void restore_focus();

    XConnection &x_;
    xcb_visualtype_t *visual_;
    uint8_t depth_ = 32;
    xcb_colormap_t colormap_;
    xcb_window_t window_;
    xcb_gcontext_t gc_;
    xcb_pixmap_t pixmap_ = XCB_NONE;
    cairo_surface_t *surface_ = nullptr;
    cairo_t *cr_ = nullptr;
    uint16_t backing_width_ = 0;
    uint16_t backing_height_ = 0;
    OutputGeometry geometry_{0, 0, 1, 1};
    bool mapped_ = false;
    bool focused_ = false;
    xcb_window_t previous_focus_ = XCB_NONE;
};

} // namespace astralia
