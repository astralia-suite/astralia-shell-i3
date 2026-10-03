#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <malloc.h>
#include <map>
#include <vector>
#include <xcb/xcb_ewmh.h>

#include "config/bar_config.h"

#include "core/log.h"

#include "modules/bar.h"
#include "modules/bar/panel/audio_panel.h"
#include "modules/bar/panel/battery_panel.h"
#include "modules/bar/panel/bluetooth_panel.h"
#include "modules/bar/panel/control_center_panel.h"
#include "modules/bar/panel/network_panel.h"
#include "modules/bar/widget/clock_widget.h"
#include "modules/bar/widget/control_center_widget.h"
#include "modules/bar/widget/logout_widget.h"
#include "modules/bar/widget/status_widget.h"
#include "modules/bar/widget/workspace_widget.h"

#include "render/app_fonts.h"
#include "render/draw.h"

namespace astralia {

Bar::Bar(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services)
    : x_(x), loop_(loop), ipc_(ipc), services_(services),
      window_(x, "astralia-shell", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_LEAVE_WINDOW, false) {
    register_app_fonts();
    clock_ = std::make_unique<ClockWidget>();
    logout_ = std::make_unique<LogoutWidget>();
    status_ = std::make_unique<StatusWidget>();
    control_center_ = std::make_unique<ControlCenterWidget>();
    OutputGeometry output = x_.primary_output();
    width_ = output.width;
    height_ = bar_config::margin_top + bar_config::height;
    panel_ = {bar_config::margin_x, bar_config::margin_top, width_ - 2 * bar_config::margin_x,
              bar_config::height};

    window_.place({output.x, output.y, width_, height_});
    set_hints(output);
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
    audio_panel_ = std::make_unique<AudioPanel>(x_, loop, services_.audio);
    battery_panel_ = std::make_unique<BatteryPanel>(x_, loop, services_.battery);
    bluetooth_panel_ = std::make_unique<BluetoothPanel>(x_, loop, services_.bluetooth);
    network_panel_ = std::make_unique<NetworkPanel>(x_, loop, services_.network);
    for (PanelWindow *panel : {&control_center_panel_->window(), &bluetooth_panel_->window(), &network_panel_->window(),
                               &audio_panel_->window(), &battery_panel_->window()}) {
        panel->changed.connect([this] { sync_panels(); });
    }

    clock_->refresh();
    status_->update(services_.bluetooth.status(), services_.network.status(),
                    services_.audio.sink(), services_.battery.status());
    draw_all();

    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) {
        switch (event.response_type & ~0x80) {
        case XCB_EXPOSE: {
            const auto &expose = reinterpret_cast<const xcb_expose_event_t &>(event);
            window_.present(expose.x, expose.y, expose.width, expose.height);
            break;
        }
        case XCB_BUTTON_PRESS:
            click(reinterpret_cast<const xcb_button_press_event_t &>(event));
            break;
        case XCB_MOTION_NOTIFY:
            hover(reinterpret_cast<const xcb_motion_notify_event_t &>(event).event_x);
            break;
        case XCB_LEAVE_NOTIFY:
            // A panel's pointer grab sends a grab-mode leave while the cursor is still on the widget, and closing it can race a stray leave, so keep the widget expanded until the linger re-checks the real position.
            if (reinterpret_cast<const xcb_leave_notify_event_t &>(event).mode == XCB_NOTIFY_MODE_NORMAL &&
                std::chrono::steady_clock::now() >= linger_until_) {
                hover(std::nullopt);
            }
            break;
        default:
            break;
        }
    });
    loop.add_timer([] { return ms_until_next_second(std::chrono::system_clock::now()); },
                   [this] { redraw_clock(); });
    loop.add_timer([] { return bar_config::trim_interval; }, [] { malloc_trim(0); });
    linger_timer_ = loop.add_timer([this] { return until_linger_end(); }, [this] { sync_hover(); });

    window_.show(false);
}

Bar::~Bar() = default;

void Bar::set_hints(const OutputGeometry &output) {
    xcb_ewmh_connection_t *ewmh = x_.ewmh();
    xcb_ewmh_set_wm_window_type(ewmh, window_.id(), 1, &ewmh->_NET_WM_WINDOW_TYPE_DOCK);
    std::array<xcb_atom_t, 2> states{ewmh->_NET_WM_STATE_STICKY, ewmh->_NET_WM_STATE_ABOVE};
    xcb_ewmh_set_wm_state(ewmh, window_.id(), states.size(), states.data());
    xcb_ewmh_set_wm_desktop(ewmh, window_.id(), 0xFFFFFFFF);

    uint32_t top = output.y + height_;
    xcb_ewmh_wm_strut_partial_t strut{};
    strut.top = top;
    strut.top_start_x = output.x;
    strut.top_end_x = output.x + width_ - 1;
    xcb_ewmh_set_wm_strut_partial(ewmh, window_.id(), strut);
    xcb_ewmh_set_wm_strut(ewmh, window_.id(), 0, 0, top, 0);
}

void Bar::paint_background(const Rect &rect) {
    cairo_set_operator(window_.cr(), CAIRO_OPERATOR_SOURCE);
    set_source(window_.cr(), palette::base_alpha80);
    cairo_rectangle(window_.cr(), rect.x, rect.y, rect.width, rect.height);
    cairo_fill(window_.cr());
    cairo_set_operator(window_.cr(), CAIRO_OPERATOR_OVER);
}

void Bar::paint_panel() {
    constexpr double inset = bar_config::border_width / 2.0;
    set_source(window_.cr(), palette::base_alpha80);
    rounded_rect(window_.cr(), panel_.x, panel_.y, panel_.width, panel_.height, bar_config::corner_radius);
    cairo_fill(window_.cr());
    set_source(window_.cr(), palette::accent);
    cairo_set_line_width(window_.cr(), bar_config::border_width);
    rounded_rect(window_.cr(), panel_.x + inset, panel_.y + inset, panel_.width - 2 * inset,
                 panel_.height - 2 * inset, bar_config::corner_radius - inset);
    cairo_stroke(window_.cr());
}

void Bar::draw_divider(const Rect &left, const Rect &right) {
    double x = (left.x + left.width + right.x) / 2.0;
    double height = panel_.height * bar_config::divider_height_ratio;
    set_source(window_.cr(), palette::text_alpha20);
    cairo_rectangle(window_.cr(), std::floor(x), panel_.y + (panel_.height - height) / 2.0,
                    bar_config::divider_width, height);
    cairo_fill(window_.cr());
}

void Bar::draw_all() {
    Rect whole{0, 0, width_, height_};
    cairo_set_operator(window_.cr(), CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(window_.cr(), 0, 0, 0, 0);
    cairo_paint(window_.cr());
    cairo_set_operator(window_.cr(), CAIRO_OPERATOR_OVER);
    paint_panel();

    set_source(window_.cr(), palette::text);
    logout_rect_ = {panel_.x + bar_config::padding_x, panel_.y, logout_->width(), panel_.height};
    logout_->draw(window_.cr(), logout_rect_.x, panel_.y, panel_.height);
    const I3Status &workspaces = services_.i3.status();
    workspace_rect_ = {logout_rect_.x + logout_rect_.width + bar_config::group_gap, panel_.y,
                       workspace_row_width(workspaces), panel_.height};
    draw_workspace_row(window_.cr(), workspaces, workspace_rect_.x, panel_.y, panel_.height);
    draw_divider(logout_rect_, workspace_rect_);

    clock_rect_ = clock_rect();
    draw_clock();

    set_source(window_.cr(), palette::text);
    int control_center_width = control_center_->width();
    control_center_rect_ = {panel_.x + panel_.width - bar_config::padding_x - control_center_width,
                            panel_.y, control_center_width, panel_.height};
    control_center_->draw(window_.cr(), control_center_rect_.x, panel_.y, panel_.height);
    int status_width = status_->width();
    status_rect_ = {control_center_rect_.x - bar_config::group_gap - status_width, panel_.y,
                    status_width, panel_.height};
    status_->draw(window_.cr(), status_rect_.x, panel_.y, panel_.height);
    draw_divider(status_rect_, control_center_rect_);
    window_.present(whole.x, whole.y, whole.width, whole.height);
}

void Bar::draw_clock() {
    set_source(window_.cr(), palette::text);
    clock_->draw(window_.cr(), panel_.x + panel_.width / 2.0, panel_.y + panel_.height / 2.0);
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
    window_.present(dirty.x, dirty.y, dirty.width, dirty.height);
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
    start_linger();
    auto toggle_only = [this](auto &panel) {
        close_panels_except(&panel.window());
        panel.toggle();
    };
    if (control_center_rect_.contains(event.event_x)) {
        toggle_only(*control_center_panel_);
    } else if (std::optional<StatusItem> item = status_rect_.contains(event.event_x)
                                                    ? status_->item_at(event.event_x - status_rect_.x)
                                                    : std::nullopt) {
        switch (*item) {
        case StatusItem::bluetooth:
            toggle_only(*bluetooth_panel_);
            break;
        case StatusItem::network:
            toggle_only(*network_panel_);
            break;
        case StatusItem::volume:
            toggle_only(*audio_panel_);
            break;
        case StatusItem::battery:
            toggle_only(*battery_panel_);
            break;
        }
    } else {
        close_panels_except(nullptr);
        if (logout_rect_.contains(event.event_x)) {
            ipc_.dispatch("logout");
        } else if (workspace_rect_.contains(event.event_x)) {
            if (auto index = workspace_at(services_.i3.status(), event.event_x - workspace_rect_.x)) {
                services_.i3.switch_to(*index);
            }
        }
    }
}

void Bar::close_panels_except(const PanelWindow *keep) {
    for (PanelWindow *panel : {&control_center_panel_->window(), &bluetooth_panel_->window(), &network_panel_->window(),
                               &audio_panel_->window(), &battery_panel_->window()}) {
        if (panel != keep) {
            panel->close();
        }
    }
}

std::optional<StatusItem> Bar::open_item() const {
    if (bluetooth_panel_->window().is_open()) {
        return StatusItem::bluetooth;
    }
    if (network_panel_->window().is_open()) {
        return StatusItem::network;
    }
    if (audio_panel_->window().is_open()) {
        return StatusItem::volume;
    }
    if (battery_panel_->window().is_open()) {
        return StatusItem::battery;
    }
    return std::nullopt;
}

// Like hl, a status item stays expanded while its panel is open; on close it lingers briefly, then falls back to whether the cursor is on it.
void Bar::sync_panels() {
    if (std::optional<StatusItem> item = open_item()) {
        if (status_->pin(item)) {
            draw_all();
        }
        return;
    }
    start_linger();
}

void Bar::start_linger() {
    linger_until_ = std::chrono::steady_clock::now() + bar_config::panel_close_linger;
    loop_.reschedule(linger_timer_);
}

void Bar::hover(std::optional<int> x, bool redraw) {
    std::optional<int> offset;
    if (x && status_rect_.contains(*x)) {
        offset = *x - status_rect_.x;
    }
    bool logout_changed = logout_->hover(x && logout_rect_.contains(*x));
    bool status_changed = status_->hover(offset);
    if (logout_changed || status_changed || redraw) {
        draw_all();
    }
}

void Bar::sync_hover() {
    if (std::chrono::steady_clock::now() < linger_until_) {
        return;
    }
    xcb_query_pointer_reply_t *reply =
        xcb_query_pointer_reply(x_.conn(), xcb_query_pointer(x_.conn(), window_.id()), nullptr);
    if (reply == nullptr) {
        return;
    }
    bool inside = reply->same_screen && reply->win_x >= 0 && reply->win_y >= 0 && reply->win_x < width_ && reply->win_y < height_;
    int x = reply->win_x;
    free(reply);
    bool pinned = status_->pin(open_item());
    hover(inside ? std::optional<int>(x) : std::nullopt, pinned);
}

std::chrono::milliseconds Bar::until_linger_end() const {
    auto left = linger_until_ - std::chrono::steady_clock::now();
    if (left < std::chrono::steady_clock::duration::zero()) {
        return std::chrono::hours(1);
    }
    return std::chrono::ceil<std::chrono::milliseconds>(left);
}

Bar::Rect Bar::clock_rect() const {
    int width = clock_->width();
    return {panel_.x + (panel_.width - width) / 2, panel_.y, width, panel_.height};
}

} // namespace astralia
