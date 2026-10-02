#include <algorithm>
#include <array>
#include <cairo-xcb.h>
#include <chrono>
#include <cmath>
#include <malloc.h>
#include <map>
#include <numbers>
#include <string_view>
#include <vector>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config/bar_config.h"

#include "core/app_fonts.h"
#include "core/log.h"

#include "modules/bar.h"
#include "modules/bar/panel/control_center_panel.h"
#include "modules/bar/widget/clock_widget.h"
#include "modules/bar/widget/control_center_widget.h"
#include "modules/bar/widget/logout_widget.h"
#include "modules/bar/widget/status_widget.h"
#include "modules/bar/widget/workspace_widget.h"

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

Bar::Bar(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services)
    : x_(x), ipc_(ipc), services_(services) {
    register_app_fonts();
    clock_ = std::make_unique<ClockWidget>();
    logout_ = std::make_unique<LogoutWidget>();
    status_ = std::make_unique<StatusWidget>();
    control_center_ = std::make_unique<ControlCenterWidget>();
    xcb_connection_t *conn = x_.conn();
    OutputGeometry output = x_.primary_output();
    width_ = output.width;
    height_ = bar_config::margin_top + bar_config::height;
    panel_ = {bar_config::margin_x, bar_config::margin_top, width_ - 2 * bar_config::margin_x,
              bar_config::height};

    xcb_visualtype_t *visual = x_.argb_visual();
    uint8_t depth = 32;
    if (visual == nullptr) {
        visual = x_.visual();
        depth = x_.screen()->root_depth;
    }
    colormap_ = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap_, x_.root(), visual->visual_id);

    window_ = xcb_generate_id(conn);
    std::array<uint32_t, 4> window_values{XCB_BACK_PIXMAP_NONE, 0,
                                          XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                                              XCB_EVENT_MASK_POINTER_MOTION |
                                              XCB_EVENT_MASK_LEAVE_WINDOW,
                                          colormap_};
    xcb_create_window(conn, depth, window_, x_.root(), output.x, output.y, width_, height_, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK |
                          XCB_CW_COLORMAP,
                      window_values.data());
    set_hints(output);

    pixmap_ = xcb_generate_id(conn);
    xcb_create_pixmap(conn, depth, pixmap_, window_, width_, height_);
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, pixmap_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);
    surface_ = cairo_xcb_surface_create(conn, pixmap_, visual, width_, height_);
    cr_ = cairo_create(surface_);

    notifier_ =
        services_.session.proxy("org.freedesktop.Notifications", "/org/freedesktop/Notifications");
    services_.i3.changed.connect([this] { draw_all(); });
    services_.bluetooth.changed.connect([this] { redraw_status(); });
    services_.bluetooth.messages.connect(
        [this](const StatusMessage &message) { notify("Bluetooth", message); });
    services_.network.changed.connect([this] { redraw_status(); });
    services_.network.messages.connect(
        [this](const StatusMessage &message) { notify("Network", message); });
    services_.audio.changed.connect([this](AudioKind kind) {
        if (kind == AudioKind::sink) {
            redraw_status();
        }
    });
    services_.battery.changed.connect([this] { redraw_status(); });
    control_center_panel_ = std::make_unique<ControlCenterPanel>(x_, loop, services_);

    clock_->refresh();
    status_->update(services_.bluetooth.status(), services_.network.status(),
                    services_.audio.sink(), services_.battery.status());
    draw_all();

    loop.on_window(window_, [this](const xcb_generic_event_t &event) {
        switch (event.response_type & ~0x80) {
        case XCB_EXPOSE: {
            const auto &expose = reinterpret_cast<const xcb_expose_event_t &>(event);
            present({expose.x, expose.y, expose.width, expose.height});
            break;
        }
        case XCB_BUTTON_PRESS:
            click(reinterpret_cast<const xcb_button_press_event_t &>(event));
            break;
        case XCB_MOTION_NOTIFY:
            hover(reinterpret_cast<const xcb_motion_notify_event_t &>(event).event_x);
            break;
        case XCB_LEAVE_NOTIFY:
            hover(std::nullopt);
            break;
        default:
            break;
        }
    });
    loop.add_timer([] { return ms_until_next_second(std::chrono::system_clock::now()); },
                   [this] { redraw_clock(); });
    loop.add_timer([] { return bar_config::trim_interval; }, [] { malloc_trim(0); });

    xcb_map_window(conn, window_);
}

Bar::~Bar() {
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
    xcb_connection_t *conn = x_.conn();
    xcb_free_gc(conn, gc_);
    xcb_free_pixmap(conn, pixmap_);
    xcb_destroy_window(conn, window_);
    xcb_free_colormap(conn, colormap_);
    xcb_flush(conn);
}

void Bar::set_hints(const OutputGeometry &output) {
    using namespace std::string_view_literals;
    xcb_ewmh_connection_t *ewmh = x_.ewmh();
    constexpr std::string_view name = "astralia-shell"sv;
    constexpr std::string_view wm_class = "astralia-shell\0astralia-shell\0"sv;
    xcb_ewmh_set_wm_name(ewmh, window_, name.size(), name.data());
    xcb_icccm_set_wm_class(x_.conn(), window_, wm_class.size(), wm_class.data());
    xcb_ewmh_set_wm_window_type(ewmh, window_, 1, &ewmh->_NET_WM_WINDOW_TYPE_DOCK);
    std::array<xcb_atom_t, 2> states{ewmh->_NET_WM_STATE_STICKY, ewmh->_NET_WM_STATE_ABOVE};
    xcb_ewmh_set_wm_state(ewmh, window_, states.size(), states.data());
    xcb_ewmh_set_wm_desktop(ewmh, window_, 0xFFFFFFFF);

    uint32_t top = output.y + height_;
    xcb_ewmh_wm_strut_partial_t strut{};
    strut.top = top;
    strut.top_start_x = output.x;
    strut.top_end_x = output.x + width_ - 1;
    xcb_ewmh_set_wm_strut_partial(ewmh, window_, strut);
    xcb_ewmh_set_wm_strut(ewmh, window_, 0, 0, top, 0);
}

void Bar::paint_background(const Rect &rect) {
    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    set_source(cr_, bar_config::background);
    cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
}

void Bar::paint_panel() {
    constexpr double inset = bar_config::border_width / 2.0;
    set_source(cr_, bar_config::background);
    rounded_rect(cr_, panel_.x, panel_.y, panel_.width, panel_.height, bar_config::corner_radius);
    cairo_fill(cr_);
    set_source(cr_, bar_config::border);
    cairo_set_line_width(cr_, bar_config::border_width);
    rounded_rect(cr_, panel_.x + inset, panel_.y + inset, panel_.width - 2 * inset,
                 panel_.height - 2 * inset, bar_config::corner_radius - inset);
    cairo_stroke(cr_);
}

void Bar::draw_divider(const Rect &left, const Rect &right) {
    double x = (left.x + left.width + right.x) / 2.0;
    double height = panel_.height * bar_config::divider_height_ratio;
    set_source(cr_, bar_config::divider);
    cairo_rectangle(cr_, std::floor(x), panel_.y + (panel_.height - height) / 2.0,
                    bar_config::divider_width, height);
    cairo_fill(cr_);
}

void Bar::draw_all() {
    Rect whole{0, 0, width_, height_};
    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr_, 0, 0, 0, 0);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
    paint_panel();

    set_source(cr_, bar_config::foreground);
    logout_rect_ = {panel_.x + bar_config::padding_x, panel_.y, logout_->width(), panel_.height};
    logout_->draw(cr_, logout_rect_.x, panel_.y, panel_.height);
    const I3Status &workspaces = services_.i3.status();
    workspace_rect_ = {logout_rect_.x + logout_rect_.width + bar_config::group_gap, panel_.y,
                       workspace_row_width(workspaces), panel_.height};
    draw_workspace_row(cr_, workspaces, workspace_rect_.x, panel_.y, panel_.height);
    draw_divider(logout_rect_, workspace_rect_);

    clock_rect_ = clock_rect();
    draw_clock();

    set_source(cr_, bar_config::foreground);
    int control_center_width = control_center_->width();
    control_center_rect_ = {panel_.x + panel_.width - bar_config::padding_x - control_center_width,
                            panel_.y, control_center_width, panel_.height};
    control_center_->draw(cr_, control_center_rect_.x, panel_.y, panel_.height);
    int status_width = status_->width();
    status_rect_ = {control_center_rect_.x - bar_config::group_gap - status_width, panel_.y,
                    status_width, panel_.height};
    status_->draw(cr_, status_rect_.x, panel_.y, panel_.height);
    draw_divider(status_rect_, control_center_rect_);
    present(whole);
}

void Bar::draw_clock() {
    set_source(cr_, bar_config::foreground);
    clock_->draw(cr_, panel_.x + panel_.width / 2.0, panel_.y + panel_.height / 2.0);
}

void Bar::redraw_clock() {
    if (!clock_->refresh()) {
        return;
    }
    Rect previous = clock_rect_;
    clock_rect_ = clock_rect();
    int left = std::min(previous.x, clock_rect_.x);
    int right = std::max(previous.x + previous.width, clock_rect_.x + clock_rect_.width);
    constexpr int border = static_cast<int>(bar_config::border_width);
    Rect dirty{left, panel_.y + border, right - left, panel_.height - 2 * border};
    paint_background(dirty);
    draw_clock();
    present(dirty);
}

void Bar::redraw_status() {
    status_->update(services_.bluetooth.status(), services_.network.status(),
                    services_.audio.sink(), services_.battery.status());
    draw_all();
}

void Bar::notify(const std::string &app, const StatusMessage &message) {
    if (!notifier_) {
        return;
    }
    try {
        notifier_->callMethod("Notify")
            .onInterface("org.freedesktop.Notifications")
            .withArguments(app, uint32_t{0}, std::string(), message.summary, message.body,
                           std::vector<std::string>{}, std::map<std::string, sdbus::Variant>{},
                           int32_t{-1})
            .dontExpectReply();
    } catch (const sdbus::Error &error) {
        log::error("bar: cannot send notification: {}", error.what());
    }
}

void Bar::click(const xcb_button_press_event_t &event) {
    if (event.detail != XCB_BUTTON_INDEX_1) {
        return;
    }
    if (logout_rect_.contains(event.event_x)) {
        ipc_.dispatch("logout");
    } else if (control_center_rect_.contains(event.event_x)) {
        control_center_panel_->toggle();
    } else if (workspace_rect_.contains(event.event_x)) {
        if (auto index = workspace_at(services_.i3.status(), event.event_x - workspace_rect_.x)) {
            services_.i3.switch_to(*index);
        }
    }
}

void Bar::hover(std::optional<int> x) {
    std::optional<int> offset;
    if (x && status_rect_.contains(*x)) {
        offset = *x - status_rect_.x;
    }
    bool logout_changed = logout_->hover(x && logout_rect_.contains(*x));
    bool status_changed = status_->hover(offset);
    if (logout_changed || status_changed) {
        draw_all();
    }
}

void Bar::present(const Rect &rect) {
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, rect.x, rect.y, rect.x, rect.y, rect.width,
                  rect.height);
}

Bar::Rect Bar::clock_rect() const {
    int width = clock_->width();
    return {panel_.x + (panel_.width - width) / 2, panel_.y, width, panel_.height};
}

} // namespace astralia
