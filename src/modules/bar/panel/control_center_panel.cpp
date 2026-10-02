#include <algorithm>
#include <cairo-xcb.h>
#include <cmath>
#include <format>
#include <numbers>
#include <string_view>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config/bar_config.h"

#include "core/icons.h"

#include "modules/bar/panel/control_center_panel.h"

namespace astralia {

namespace {

void set_source(cairo_t *cr, const Color &color) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}

void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    r = std::min({r, w / 2.0, h / 2.0});
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -std::numbers::pi / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, std::numbers::pi / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, std::numbers::pi / 2.0, std::numbers::pi);
    cairo_arc(cr, x + r, y + r, r, std::numbers::pi, 3.0 * std::numbers::pi / 2.0);
    cairo_close_path(cr);
}

} // namespace

int slider_percent_at(int track_x, int track_width, int px) {
    if (track_width <= 0) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround((px - track_x) * 100.0 / track_width)), 0, 100);
}

ControlCenterPanel::Slider::Slider() : icon(bar_config::icon_font), label(bar_config::font) {}

ControlCenterPanel::ControlCenterPanel(XConnection &x, EventLoop &loop, Services &services)
    : x_(x), keyboard_(x.conn()), brightness_(services.brightness),
      audio_(services.audio),
      percent_sample_(bar_config::font) {
    percent_sample_.set(bar_config::control_center_percent_sample);
    width_ = bar_config::control_center_width;
    height_ = 2 * bar_config::control_center_padding +
              static_cast<int>(sliders_.size()) * bar_config::control_center_row_height;

    xcb_connection_t *conn = x_.conn();
    xcb_visualtype_t *visual = x_.argb_visual();
    uint8_t depth = 32;
    if (visual == nullptr) {
        visual = x_.visual();
        depth = x_.screen()->root_depth;
    }
    colormap_ = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap_, x_.root(), visual->visual_id);
    window_ = xcb_generate_id(conn);
    std::array<uint32_t, 5> values{XCB_BACK_PIXMAP_NONE, 0, 1,
                                   XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                                       XCB_EVENT_MASK_BUTTON_RELEASE |
                                       XCB_EVENT_MASK_BUTTON_1_MOTION | XCB_EVENT_MASK_KEY_PRESS |
                                       XCB_EVENT_MASK_FOCUS_CHANGE,
                                   colormap_};
    xcb_create_window(conn, depth, window_, x_.root(), 0, 0, width_, height_, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      values.data());
    using namespace std::string_view_literals;
    constexpr std::string_view name = "astralia-control-center"sv;
    constexpr std::string_view wm_class = "astralia-control-center\0astralia-shell\0"sv;
    xcb_ewmh_set_wm_name(x_.ewmh(), window_, name.size(), name.data());
    xcb_icccm_set_wm_class(conn, window_, wm_class.size(), wm_class.data());

    pixmap_ = xcb_generate_id(conn);
    xcb_create_pixmap(conn, depth, pixmap_, window_, width_, height_);
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, pixmap_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);
    surface_ = cairo_xcb_surface_create(conn, pixmap_, visual, width_, height_);
    cr_ = cairo_create(surface_);

    loop.on_window(window_, [this](const xcb_generic_event_t &event) { handle(event); });
    brightness_.changed.connect([this] {
        if (open_ && dragging_ != brightness) {
            sync_brightness();
            paint();
        }
    });
    audio_.changed.connect([this](AudioKind kind) {
        if (kind == AudioKind::sink && open_ && dragging_ != volume) {
            sync_volume();
            paint();
        }
    });
}

ControlCenterPanel::~ControlCenterPanel() {
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
    xcb_connection_t *conn = x_.conn();
    xcb_free_gc(conn, gc_);
    xcb_free_pixmap(conn, pixmap_);
    xcb_destroy_window(conn, window_);
    xcb_free_colormap(conn, colormap_);
    xcb_flush(conn);
}

void ControlCenterPanel::toggle() {
    if (open_) {
        close();
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

    OutputGeometry output = x_.primary_output();
    std::array<uint32_t, 3> values{
        static_cast<uint32_t>(output.x + output.width - bar_config::margin_x - width_),
        static_cast<uint32_t>(output.y + 2 * bar_config::margin_top + bar_config::height),
        XCB_STACK_MODE_ABOVE};
    xcb_connection_t *conn = x_.conn();
    xcb_configure_window(conn, window_,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE,
                         values.data());
    keyboard_.reload();
    open_ = true;
    paint();
    xcb_map_window(conn, window_);
    take_focus();
    xcb_flush(conn);
}

void ControlCenterPanel::close() {
    xcb_connection_t *conn = x_.conn();
    restore_focus();
    xcb_unmap_window(conn, window_);
    xcb_flush(conn);
    open_ = false;
    dragging_.reset();
}

void ControlCenterPanel::take_focus() {
    xcb_connection_t *conn = x_.conn();
    xcb_get_input_focus_reply_t *reply =
        xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    previous_focus_ = reply != nullptr ? reply->focus : XCB_NONE;
    free(reply);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, window_, XCB_CURRENT_TIME);
}

void ControlCenterPanel::restore_focus() {
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
}

void ControlCenterPanel::handle(const xcb_generic_event_t &event) {
    if (!open_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        present();
        break;
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        if (keyboard_.press(key.detail, key.state).kind == KeyKind::escape) {
            close();
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
    case XCB_FOCUS_OUT: {
        const auto &focus = reinterpret_cast<const xcb_focus_out_event_t &>(event);
        if (focus.mode == XCB_NOTIFY_MODE_NORMAL && focus.detail != XCB_NOTIFY_DETAIL_POINTER) {
            close();
        }
        break;
    }
    default:
        break;
    }
}

void ControlCenterPanel::press(int x, int y, xcb_button_t button) {
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
    int offset = y - bar_config::control_center_padding;
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
    return bar_config::control_center_padding + bar_config::control_center_icon_slot +
           bar_config::control_center_gap;
}

int ControlCenterPanel::track_width() const {
    return width_ - bar_config::control_center_padding - percent_sample_.width() -
           bar_config::control_center_gap - track_x();
}

void ControlCenterPanel::paint() {
    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr_, 0, 0, 0, 0);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);

    constexpr double inset = bar_config::border_width / 2.0;
    set_source(cr_, bar_config::background);
    rounded_rect(cr_, 0, 0, width_, height_, metrics::radius_md);
    cairo_fill(cr_);
    set_source(cr_, bar_config::border);
    cairo_set_line_width(cr_, bar_config::border_width);
    rounded_rect(cr_, inset, inset, width_ - 2 * inset, height_ - 2 * inset,
                 metrics::radius_md - inset);
    cairo_stroke(cr_);

    Slider &sound = sliders_[volume];
    Slider &light = sliders_[brightness];
    light.icon.set(icon::brightness_threshold(light.percent));
    sound.icon.set(icon::volume_threshold(sound.muted, sound.percent));
    int track_left = track_x();
    int track_span = track_width();
    constexpr int track_height = bar_config::control_center_track_height;
    for (std::size_t i = 0; i < sliders_.size(); ++i) {
        Slider &slider = sliders_[i];
        if (!slider.enabled) {
            slider.label.set("-");
        } else if (slider.muted) {
            slider.label.set("muted");
        } else {
            slider.label.set(std::format("{}%", slider.percent));
        }
        int top = bar_config::control_center_padding +
                  static_cast<int>(i) * bar_config::control_center_row_height;
        double center_y = top + bar_config::control_center_row_height / 2.0;
        set_source(cr_, !slider.enabled ? palette::text_dim
                        : slider.muted  ? palette::text_muted
                                        : bar_config::foreground);
        slider.icon.draw_centered(cr_, bar_config::control_center_padding, top,
                                  bar_config::control_center_row_height);
        slider.label.draw_centered(cr_, width_ - bar_config::control_center_padding - slider.label.width(),
                                   top, bar_config::control_center_row_height);
        double track_top = center_y - track_height / 2.0;
        set_source(cr_, bar_config::control_center_track);
        rounded_rect(cr_, track_left, track_top, track_span, track_height, track_height / 2.0);
        cairo_fill(cr_);
        if (slider.enabled && slider.percent > 0) {
            set_source(cr_, slider.muted ? palette::text_muted : bar_config::control_center_fill);
            rounded_rect(cr_, track_left, track_top, track_span * slider.percent / 100.0,
                         track_height, track_height / 2.0);
            cairo_fill(cr_);
        }
    }
    present();
}

void ControlCenterPanel::present() {
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, 0, 0, 0, 0, width_, height_);
    xcb_flush(x_.conn());
}

} // namespace astralia
