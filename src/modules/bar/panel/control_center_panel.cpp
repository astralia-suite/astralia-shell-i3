#include <algorithm>
#include <format>

#include "config/bar_config.h"
#include "config/panel_config.h"

#include "modules/bar/panel/control_center_panel.h"

#include "render/icons.h"

namespace astralia {

ControlCenterPanel::Slider::Slider() : icon(bar_config::icon_font), label(bar_config::font) {}

ControlCenterPanel::ControlCenterPanel(XConnection &x, EventLoop &loop, Services &services)
    : brightness_(services.brightness), audio_(services.audio),
      percent_sample_(bar_config::font),
      window_(x, loop, "astralia-control-center", panel_config::width, static_cast<int>(panel_content_top()) + panel_config::padding + static_cast<int>(sliders_.size()) * bar_config::control_center_row_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] { dragging_.reset(); }) {
    percent_sample_.set(bar_config::control_center_percent_sample);
    brightness_.changed.connect([this] {
        if (window_.is_open() && dragging_ != brightness) {
            sync_brightness();
            paint();
        }
    });
    audio_.changed.connect([this](AudioKind kind) {
        if (kind == AudioKind::sink && window_.is_open() && dragging_ != volume) {
            sync_volume();
            paint();
        }
    });
}

void ControlCenterPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
    } else {
        open();
    }
}

void ControlCenterPanel::sync_brightness() {
    Slider &light = sliders_[brightness];
    light.enabled = brightness_.available();
    light.percent = brightness_.percent();
}

void ControlCenterPanel::sync_volume() {
    Slider &sound = sliders_[volume];
    AudioLevel level = audio_.sink();
    sound.enabled = level.present;
    sound.percent = std::min(level.percent, 100);
    sound.muted = level.muted;
}

void ControlCenterPanel::open() {
    sync_brightness();
    sync_volume();
    paint();
    window_.open(bar_config::margin_x, bar_config::panel_top);
}

void ControlCenterPanel::handle(const xcb_generic_event_t &event) {
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
        if (reinterpret_cast<const xcb_button_release_event_t &>(event).detail ==
            XCB_BUTTON_INDEX_1) {
            dragging_.reset();
        }
        break;
    case XCB_MOTION_NOTIFY:
        if (dragging_) {
            const auto &motion = reinterpret_cast<const xcb_motion_notify_event_t &>(event);
            apply(*dragging_, slider_percent_at(track_x(), track_width(), motion.event_x));
        }
        break;
    default:
        break;
    }
}

void ControlCenterPanel::press(int x, int y, xcb_button_t button) {
    if (button == XCB_BUTTON_INDEX_1 && panel_hit_at(hits_, x, y)) {
        window_.close();
        return;
    }
    std::optional<Row> row = row_at(y);
    if (!row || !sliders_[*row].enabled) {
        return;
    }
    int percent = sliders_[*row].percent;
    switch (button) {
    case XCB_BUTTON_INDEX_1:
        if (*row == volume && x < track_x()) {
            sliders_[volume].muted = !sliders_[volume].muted;
            audio_.set_sink_mute(sliders_[volume].muted);
            paint();
            break;
        }
        dragging_ = row;
        apply(*row, slider_percent_at(track_x(), track_width(), x));
        break;
    case XCB_BUTTON_INDEX_4:
        apply(*row, std::min(percent + bar_config::control_center_wheel_step, 100));
        break;
    case XCB_BUTTON_INDEX_5:
        apply(*row, std::max(percent - bar_config::control_center_wheel_step, 0));
        break;
    default:
        break;
    }
}

void ControlCenterPanel::apply(Row row, int percent) {
    if (percent == sliders_[row].percent) {
        return;
    }
    sliders_[row].percent = percent;
    if (row == brightness) {
        brightness_.set(percent);
    } else {
        audio_.set_sink_volume(percent);
    }
    paint();
}

std::optional<ControlCenterPanel::Row> ControlCenterPanel::row_at(int y) const {
    int offset = y - static_cast<int>(panel_content_top());
    if (offset < 0) {
        return std::nullopt;
    }
    std::size_t index = static_cast<std::size_t>(offset / bar_config::control_center_row_height);
    if (index >= sliders_.size()) {
        return std::nullopt;
    }
    return static_cast<Row>(index);
}

int ControlCenterPanel::track_x() const {
    return panel_config::padding + bar_config::control_center_icon_slot +
           bar_config::control_center_gap;
}

int ControlCenterPanel::track_width() const {
    return window_.width() - panel_config::padding - percent_sample_.width() -
           bar_config::control_center_gap - track_x();
}

void ControlCenterPanel::paint() {
    cairo_t *cr = window_.cr();
    int width = window_.width();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, window_.height());
    hits_.clear();
    panel_draw_header(cr, width, "Control Center", hits_, 1);

    Slider &sound = sliders_[volume];
    Slider &light = sliders_[brightness];
    light.icon.set(icon::brightness_threshold(light.percent));
    sound.icon.set(icon::volume_threshold(sound.muted, sound.percent));
    int track_left = track_x();
    int track_span = track_width();
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        Slider &slider = sliders_[i];
        if (!slider.enabled) {
            slider.label.set("-");
        } else if (slider.muted) {
            slider.label.set("muted");
        } else {
            slider.label.set(std::format("{}%", slider.percent));
        }
        int top = static_cast<int>(panel_content_top()) +
                  static_cast<int>(i) * bar_config::control_center_row_height;
        set_source(cr, !slider.enabled ? palette::text_dim
                       : slider.muted  ? palette::text_muted
                                       : bar_config::foreground);
        slider.icon.draw_centered(cr, panel_config::padding, top,
                                  bar_config::control_center_row_height);
        slider.label.draw_centered(cr, width - panel_config::padding - slider.label.width(),
                                   top, bar_config::control_center_row_height);
        PanelRect track{static_cast<double>(track_left), static_cast<double>(top),
                        static_cast<double>(track_span),
                        static_cast<double>(bar_config::control_center_row_height)};
        panel_draw_slider(cr, track, slider.enabled ? slider.percent : 0, slider.muted);
    }
    window_.present();
}

} // namespace astralia
