#include <algorithm>
#include <format>

#include "config/bar_config.h"

#include "modules/bar/panel/brightness_panel.h"

#include "render/icons.h"
#include "render/slider.h"

namespace astralia {

BrightnessPanel::BrightnessPanel(XConnection &x, EventLoop &loop, BrightnessService &brightness)
    : brightness_(brightness), icon_(bar_config::icon_font), label_(bar_config::font),
      percent_sample_(bar_config::font),
      window_(x, loop, "astralia-brightness", bar_config::panel_width, static_cast<int>(panel_content_top()) + panel_config::padding + bar_config::brightness_row_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] {
          dragging_ = false;
          hovered_ = false; }) {
    percent_sample_.set(bar_config::brightness_percent_sample);
    brightness_.changed.connect([this] {
        if (window_.is_open() && !dragging_) {
            sync();
            paint();
        }
    });
}

void BrightnessPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
    } else {
        open();
    }
}

void BrightnessPanel::sync() {
    enabled_ = brightness_.available();
    percent_ = brightness_.percent();
}

void BrightnessPanel::open() {
    sync();
    paint();
    window_.open(bar_config::margin_x, bar_config::panel_top);
}

void BrightnessPanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        if (window_.keyboard().press(key.detail, key.state).kind == KeyKind::escape) {
            window_.close();
        }
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        press(button.event_x, button.event_y, button.detail);
        break;
    }
    case XCB_BUTTON_RELEASE:
        if (const auto &release = reinterpret_cast<const xcb_button_release_event_t &>(event); release.detail == XCB_BUTTON_INDEX_1 && dragging_) {
            dragging_ = false;
            hovered_ = enabled_ && on_row(release.event_y);
            paint();
        }
        break;
    case XCB_MOTION_NOTIFY: {
        const auto &motion = reinterpret_cast<const xcb_motion_notify_event_t &>(event);
        if (dragging_) {
            apply(slider_percent_at(track_x(), track_width(), motion.event_x));
            break;
        }
        hover(enabled_ && on_row(motion.event_y));
        break;
    }
    case XCB_LEAVE_NOTIFY:
        if (!dragging_) {
            hover(false);
        }
        break;
    default:
        break;
    }
}

void BrightnessPanel::hover(bool hovered) {
    if (hovered != hovered_) {
        hovered_ = hovered;
        paint();
    }
}

void BrightnessPanel::press(int x, int y, xcb_button_t button) {
    if (button == XCB_BUTTON_INDEX_1 && panel_hit_at(hits_, x, y)) {
        window_.close();
        return;
    }
    if (!enabled_ || !on_row(y)) {
        return;
    }
    switch (button) {
    case XCB_BUTTON_INDEX_1:
        dragging_ = true;
        apply(slider_percent_at(track_x(), track_width(), x));
        break;
    case XCB_BUTTON_INDEX_4:
        apply(std::min(percent_ + bar_config::brightness_wheel_step, 100));
        break;
    case XCB_BUTTON_INDEX_5:
        apply(std::max(percent_ - bar_config::brightness_wheel_step, 0));
        break;
    default:
        break;
    }
}

void BrightnessPanel::apply(int percent) {
    if (percent == percent_) {
        return;
    }
    percent_ = percent;
    brightness_.set(percent);
    paint();
}

bool BrightnessPanel::on_row(int y) const {
    int offset = y - static_cast<int>(panel_content_top());
    return offset >= 0 && offset < bar_config::brightness_row_height;
}

int BrightnessPanel::track_x() const {
    return panel_config::padding + bar_config::brightness_icon_slot +
           bar_config::brightness_gap;
}

int BrightnessPanel::track_width() const {
    return window_.width() - panel_config::padding - percent_sample_.width() -
           bar_config::brightness_gap - track_x();
}

void BrightnessPanel::paint() {
    cairo_t *cr = window_.cr();
    int width = window_.width();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, window_.height());
    hits_.clear();
    panel_draw_header(cr, width, "Brightness", hits_, 1);

    icon_.set(icon::brightness_threshold(percent_));
    label_.set(enabled_ ? std::format("{}%", percent_) : "-");
    int top = static_cast<int>(panel_content_top());
    set_source(cr, enabled_ ? palette::text : palette::text_dim);
    icon_.draw_centered(cr, panel_config::padding, top, bar_config::brightness_row_height);
    label_.draw_centered(cr, width - panel_config::padding - label_.width(), top,
                         bar_config::brightness_row_height);
    PanelRect track{static_cast<double>(track_x()), static_cast<double>(top),
                    static_cast<double>(track_width()),
                    static_cast<double>(bar_config::brightness_row_height)};
    draw_slider(cr, track, enabled_ ? percent_ : 0, false, enabled_ && (dragging_ || hovered_));
    window_.present();
}

} // namespace astralia
