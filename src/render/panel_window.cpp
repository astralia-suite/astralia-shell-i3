#include <algorithm>

#include "render/panel_window.h"

namespace astralia {

PanelWindow::PanelWindow(XConnection &x, EventLoop &loop, std::string_view name, int width, int max_height, Handler handler, std::function<void()> closed)
    : x_(x), keyboard_(x.conn()), handler_(std::move(handler)), closed_(std::move(closed)),
      width_(width), height_(max_height), max_height_(max_height),
      window_(x, name,
              XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                  XCB_EVENT_MASK_BUTTON_1_MOTION | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE) {
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
}

cairo_t *PanelWindow::cr() {
    if (window_.cr() == nullptr) {
        place();
    }
    return window_.cr();
}

void PanelWindow::place() {
    OutputGeometry geometry{anchor_.x, anchor_.y, static_cast<uint16_t>(width_), static_cast<uint16_t>(height_)};
    window_.place(geometry, static_cast<uint16_t>(width_), static_cast<uint16_t>(max_height_));
}

void PanelWindow::open(int right_margin, int top) {
    OutputGeometry output = x_.primary_output();
    anchor_.x = static_cast<int16_t>(output.x + output.width - right_margin - width_);
    anchor_.y = static_cast<int16_t>(output.y + top);
    place();
    keyboard_.reload();
    open_ = true;
    window_.show(true);
    window_.present();
}

void PanelWindow::close() {
    if (!open_) {
        return;
    }
    window_.hide();
    window_.release();
    open_ = false;
    if (closed_) {
        closed_();
    }
}

void PanelWindow::set_height(int height) {
    height = std::clamp(height, 1, max_height_);
    if (height == height_) {
        return;
    }
    height_ = height;
    if (open_) {
        place();
    }
}

void PanelWindow::clear() {
    cr();
    window_.clear();
}

void PanelWindow::present() { window_.present(); }

void PanelWindow::handle(const xcb_generic_event_t &event) {
    if (!open_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        present();
        break;
    case XCB_FOCUS_OUT: {
        const auto &focus = reinterpret_cast<const xcb_focus_out_event_t &>(event);
        if (focus.mode == XCB_NOTIFY_MODE_NORMAL && focus.detail != XCB_NOTIFY_DETAIL_POINTER) {
            close();
        }
        break;
    }
    default:
        handler_(event);
        break;
    }
}

} // namespace astralia
