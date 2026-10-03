#include <array>
#include <cairo-xcb.h>
#include <cstdlib>
#include <string>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "render/x_window.h"

namespace astralia {

XWindow::XWindow(XConnection &x, std::string_view name, uint32_t event_mask, bool override_redirect)
    : x_(x), visual_(x.argb_visual()) {
    xcb_connection_t *conn = x_.conn();
    if (visual_ == nullptr) {
        visual_ = x_.visual();
        depth_ = x_.screen()->root_depth;
    }
    colormap_ = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap_, x_.root(), visual_->visual_id);
    window_ = xcb_generate_id(conn);
    std::array<uint32_t, 5> values{XCB_BACK_PIXMAP_NONE, 0, override_redirect ? 1u : 0u, event_mask, colormap_};
    xcb_create_window(conn, depth_, window_, x_.root(), geometry_.x, geometry_.y, geometry_.width,
                      geometry_.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      values.data());
    std::string wm_class = std::string(name) + '\0' + "astralia-shell" + '\0';
    xcb_ewmh_set_wm_name(x_.ewmh(), window_, name.size(), name.data());
    xcb_icccm_set_wm_class(conn, window_, wm_class.size(), wm_class.data());
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, window_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);
}

XWindow::~XWindow() {
    xcb_connection_t *conn = x_.conn();
    if (focused_) {
        restore_focus();
    }
    release();
    xcb_free_gc(conn, gc_);
    xcb_destroy_window(conn, window_);
    xcb_free_colormap(conn, colormap_);
    xcb_flush(conn);
}

void XWindow::place(const OutputGeometry &geometry, uint16_t backing_width, uint16_t backing_height) {
    xcb_connection_t *conn = x_.conn();
    uint16_t width = backing_width > 0 ? backing_width : geometry.width;
    uint16_t height = backing_height > 0 ? backing_height : geometry.height;
    if (cr_ == nullptr || width != backing_width_ || height != backing_height_) {
        release();
        pixmap_ = xcb_generate_id(conn);
        xcb_create_pixmap(conn, depth_, pixmap_, window_, width, height);
        surface_ = cairo_xcb_surface_create(conn, pixmap_, visual_, width, height);
        cr_ = cairo_create(surface_);
        backing_width_ = width;
        backing_height_ = height;
    }
    if (geometry != geometry_) {
        std::array<uint32_t, 4> values{static_cast<uint32_t>(geometry.x),
                                       static_cast<uint32_t>(geometry.y), geometry.width,
                                       geometry.height};
        xcb_configure_window(conn, window_,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             values.data());
        geometry_ = geometry;
    }
}

void XWindow::release() {
    if (cr_ == nullptr) {
        return;
    }
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
    xcb_free_pixmap(x_.conn(), pixmap_);
    cr_ = nullptr;
    surface_ = nullptr;
    pixmap_ = XCB_NONE;
    backing_width_ = 0;
    backing_height_ = 0;
}

void XWindow::show(bool focus) {
    xcb_connection_t *conn = x_.conn();
    uint32_t above = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, window_, XCB_CONFIG_WINDOW_STACK_MODE, &above);
    if (!mapped_) {
        xcb_map_window(conn, window_);
        mapped_ = true;
    }
    if (focus) {
        this->focus();
    }
    xcb_flush(conn);
}

void XWindow::focus() {
    if (!focused_) {
        take_focus();
    } else {
        xcb_set_input_focus(x_.conn(), XCB_INPUT_FOCUS_POINTER_ROOT, window_, XCB_CURRENT_TIME);
    }
    xcb_flush(x_.conn());
}

void XWindow::hide() {
    xcb_connection_t *conn = x_.conn();
    if (focused_) {
        restore_focus();
    }
    if (mapped_) {
        xcb_unmap_window(conn, window_);
        mapped_ = false;
    }
    xcb_flush(conn);
}

void XWindow::clear() {
    cairo_reset_clip(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr_, 0, 0, 0, 0);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
}

void XWindow::present() { present(0, 0, geometry_.width, geometry_.height); }

void XWindow::present(int x, int y, int width, int height) {
    if (cr_ == nullptr) {
        return;
    }
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, x, y, x, y, width, height);
    xcb_flush(x_.conn());
}

void XWindow::take_focus() {
    xcb_connection_t *conn = x_.conn();
    xcb_get_input_focus_reply_t *reply =
        xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    previous_focus_ = reply != nullptr ? reply->focus : XCB_NONE;
    free(reply);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, window_, XCB_CURRENT_TIME);
    focused_ = true;
}

void XWindow::restore_focus() {
    xcb_connection_t *conn = x_.conn();
    xcb_get_input_focus_reply_t *reply =
        xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    bool focused = reply != nullptr && reply->focus == window_;
    free(reply);
    if (focused) {
        xcb_window_t target = previous_focus_ != XCB_NONE && previous_focus_ != window_
                                  ? previous_focus_
                                  : static_cast<xcb_window_t>(XCB_INPUT_FOCUS_POINTER_ROOT);
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, target, XCB_CURRENT_TIME);
    }
    previous_focus_ = XCB_NONE;
    focused_ = false;
}

} // namespace astralia
