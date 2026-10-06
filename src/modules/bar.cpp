#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <malloc.h>
#include <vector>
#include <xcb/xcb_ewmh.h>

#include "config/bar_config.h"

#include "modules/bar.h"
#include "modules/bar/panel/battery_panel.h"
#include "modules/bar/panel/bluetooth_panel.h"
#include "modules/bar/panel/brightness_panel.h"
#include "modules/bar/panel/clock_panel.h"
#include "modules/bar/panel/media_panel.h"
#include "modules/bar/panel/network_panel.h"
#include "modules/bar/panel/tray_panel.h"
#include "modules/bar/panel/volume_panel.h"
#include "modules/bar/styles/continuous.h"
#include "modules/bar/styles/okinami.h"
#include "modules/bar/widget/battery_widget.h"
#include "modules/bar/widget/bluetooth_widget.h"
#include "modules/bar/widget/brightness_widget.h"
#include "modules/bar/widget/clock_widget.h"
#include "modules/bar/widget/logout_widget.h"
#include "modules/bar/widget/media_widget.h"
#include "modules/bar/widget/network_widget.h"
#include "modules/bar/widget/tray_widget.h"
#include "modules/bar/widget/volume_widget.h"
#include "modules/bar/widget/workspace_widget.h"

#include "render/app_fonts.h"
#include "render/draw.h"

namespace astralia {

Bar::Bar(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services, const Output &output)
    : x_(x), loop_(loop), ipc_(ipc), services_(services),
      window_(x, "astralia-shell", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_LEAVE_WINDOW, false) {
    register_app_fonts();
    clock_ = std::make_unique<ClockWidget>();
    media_ = std::make_unique<MediaWidget>();
    logout_ = std::make_unique<LogoutWidget>();
    bluetooth_ = std::make_unique<BluetoothWidget>();
    network_ = std::make_unique<NetworkWidget>();
    brightness_ = std::make_unique<BrightnessWidget>();
    volume_ = std::make_unique<VolumeWidget>();
    battery_ = std::make_unique<BatteryWidget>();
    tray_ = std::make_unique<TrayWidget>();
    items_ = {tray_.get(), network_.get(), bluetooth_.get(), volume_.get(), brightness_.get(), battery_.get(), media_.get(), clock_.get()};
    refresh_style();
    apply_output(output.geometry);
    services_.i3.changed.connect([this] { draw_all(); });
    auto update = [this](Item item) {
        refresh(item);
        draw_all();
    };
    services_.bluetooth.changed.connect([update] { update(bluetooth); });
    services_.network.changed.connect([update] { update(network); });
    services_.audio.changed.connect([update](AudioKind kind) {
        if (kind == AudioKind::sink) {
            update(volume);
        }
    });
    services_.battery.changed.connect([update] { update(battery); });
    services_.brightness.changed.connect([update] { update(brightness); });
    brightness_panel_ = std::make_unique<BrightnessPanel>(x_, loop, services_.brightness);
    volume_panel_ = std::make_unique<VolumePanel>(x_, loop, services_.audio);
    battery_panel_ = std::make_unique<BatteryPanel>(x_, loop, services_.battery);
    bluetooth_panel_ = std::make_unique<BluetoothPanel>(x_, loop, services_.bluetooth);
    network_panel_ = std::make_unique<NetworkPanel>(x_, loop, services_.network);
    media_panel_ = std::make_unique<MediaPanel>(x_, loop, services_.media);
    clock_panel_ = std::make_unique<ClockPanel>(x_, loop);
    tray_panel_ = std::make_unique<TrayPanel>(x_, loop, services_.tray);
    for (std::size_t i = 0; i < item_count; ++i) {
        PanelWindow &panel = panel_window(static_cast<Item>(i));
        panel.changed.connect([this] { sync_panels(); });
        refresh(static_cast<Item>(i));
    }

    apply_panel_geometry(output.geometry);
    clock_->refresh();
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

bool Bar::refresh_style() {
    BarStyle next = services_.settings.bar_style();
    if (spec_ != nullptr && next == style_) {
        return false;
    }
    style_ = next;
    spec_ = &bar_style_spec(next);
    height_ = static_cast<uint16_t>(bar_window_height(*spec_));
    return true;
}

void Bar::apply_output(const OutputGeometry &output) {
    width_ = output.width;
    panel_ = bar_panel_rect(*spec_, width_);
    window_.place({output.x, output.y, width_, height_});
    set_hints(output);
}

void Bar::apply_panel_geometry(const OutputGeometry &output) {
    for (std::size_t i = 0; i < item_count; ++i) {
        PanelWindow &panel = panel_window(static_cast<Item>(i));
        panel.set_output(output);
        panel.set_top(bar_panel_top(*spec_));
    }
}

void Bar::place(const Output &output) {
    bool restyled = refresh_style();
    if (!restyled && window_.mapped() && window_.geometry() == OutputGeometry{output.geometry.x, output.geometry.y, output.geometry.width, height_}) {
        return;
    }
    close_panels_except(nullptr);
    apply_output(output.geometry);
    apply_panel_geometry(output.geometry);
    draw_all();
    window_.show(false);
}

void Bar::hide() {
    close_panels_except(nullptr);
    hover(std::nullopt);
    window_.hide();
}

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

void Bar::paint_frame() {
    if (!bar_style_has_rail(*spec_)) {
        paint_continuous(window_.cr(), *spec_, panel_);
        return;
    }
    OkinamiFrame frame = bar_okinami_frame(*spec_, left_end_, center_span_, right_start_, width_);
    paint_okinami(window_.cr(), *spec_, width_, panel_.height, frame);
}

void Bar::add_divider(const Rect &left, const Rect &right) {
    dividers_.push_back((left.x + left.width + right.x) / 2.0);
}

void Bar::draw_divider(double x) {
    double height = panel_.height * bar_config::divider_height_ratio;
    set_source(window_.cr(), palette::text_alpha20);
    cairo_rectangle(window_.cr(), std::floor(x), panel_.y + (panel_.height - height) / 2.0,
                    bar_config::divider_width, height);
    cairo_fill(window_.cr());
}

void Bar::layout() {
    dividers_.clear();
    constexpr int pad = bar_config::pill_pad;
    constexpr int gap = bar_config::item_gap;
    const double padding = spec_->padding;
    logout_rect_ = {panel_.x + spec_->padding, panel_.y, logout_->width() + 2 * pad, panel_.height};
    const I3Status &workspaces = services_.i3.status();
    workspace_rect_ = {logout_rect_.x + logout_rect_.width + gap, panel_.y,
                       workspace_row_width(workspaces) + workspace_overview_width() + 2 * pad, panel_.height};
    add_divider(logout_rect_, workspace_rect_);
    left_end_ = std::round(workspace_rect_.x + workspace_rect_.width + padding);

    int media_width = media_->width() + 2 * pad;
    int clock_width = clock_->width() + 2 * pad;
    int center_x = panel_.x + (panel_.width - media_width - gap - clock_width) / 2;
    item_rects_[media] = {center_x, panel_.y, media_width, panel_.height};
    item_rects_[clock] = {center_x + media_width + gap, panel_.y, clock_width, panel_.height};
    add_divider(item_rects_[media], item_rects_[clock]);
    center_span_ = IslandSpan{std::round(item_rects_[media].x - padding),
                              std::round(item_rects_[clock].x + item_rects_[clock].width + padding)};

    int right = panel_.x + panel_.width - spec_->padding;
    const Rect *right_neighbor = nullptr;
    right_start_.reset();
    for (std::size_t i = media; i-- > 0;) {
        WidgetCapsule &item = *items_[i];
        int item_width = item.visible() ? item.width() + 2 * pad : 0;
        item_rects_[i] = {right - item_width, panel_.y, item_width, panel_.height};
        if (item.visible()) {
            if (right_neighbor != nullptr) {
                add_divider(item_rects_[i], *right_neighbor);
            }
            right_neighbor = &item_rects_[i];
            right_start_ = std::round(item_rects_[i].x - padding);
            right -= item_width + gap;
        }
    }
}

void Bar::draw_all() {
    cairo_set_operator(window_.cr(), CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(window_.cr(), 0, 0, 0, 0);
    cairo_paint(window_.cr());
    cairo_set_operator(window_.cr(), CAIRO_OPERATOR_OVER);
    layout();
    paint_frame();

    set_source(window_.cr(), palette::text);
    logout_->draw(window_.cr(), logout_rect_.x + bar_config::pill_pad, panel_.y, panel_.height);
    draw_workspace_row(window_.cr(), services_.i3.status(), workspace_rect_.x + bar_config::pill_pad, panel_.y, panel_.height);
    set_source(window_.cr(), palette::text);
    media_->draw(window_.cr(), item_rects_[media].x + bar_config::pill_pad, panel_.y, panel_.height);
    clock_->draw(window_.cr(), item_rects_[clock].x + bar_config::pill_pad, panel_.y, panel_.height);
    for (std::size_t i = media; i-- > 0;) {
        if (items_[i]->visible()) {
            set_source(window_.cr(), palette::text);
            items_[i]->draw(window_.cr(), item_rects_[i].x + bar_config::pill_pad, panel_.y, panel_.height);
        }
    }
    for (double x : dividers_) {
        draw_divider(x);
    }
    window_.present(0, 0, width_, height_);
}

void Bar::redraw_clock() {
    if (!clock_->refresh()) {
        return;
    }
    const Rect &rect = item_rects_[clock];
    if (bar_style_has_rail(*spec_) || clock_->width() + 2 * bar_config::pill_pad != rect.width) {
        draw_all();
        return;
    }
    constexpr int border = static_cast<int>(bar_config::border_width);
    int top = bar_style_has_rail(*spec_) ? 0 : border;
    Rect dirty{rect.x, panel_.y + top, rect.width, panel_.height - top - border};
    paint_background(dirty);
    set_source(window_.cr(), palette::text);
    clock_->draw(window_.cr(), rect.x + bar_config::pill_pad, panel_.y, panel_.height);
    window_.present(dirty.x, dirty.y, dirty.width, dirty.height);
}

void Bar::refresh(Item item) {
    switch (item) {
    case bluetooth:
        bluetooth_->update(services_.bluetooth.status());
        break;
    case network:
        network_->update(services_.network.status());
        break;
    case brightness:
        brightness_->update(services_.brightness);
        break;
    case volume:
        volume_->update(services_.audio.sink());
        break;
    case battery:
        battery_->update(services_.battery.status());
        break;
    case tray:
    case media:
    case clock:
    case item_count:
        break;
    }
}

PanelWindow &Bar::panel_window(Item item) {
    switch (item) {
    case tray:
        return tray_panel_->window();
    case bluetooth:
        return bluetooth_panel_->window();
    case network:
        return network_panel_->window();
    case brightness:
        return brightness_panel_->window();
    case volume:
        return volume_panel_->window();
    case media:
        return media_panel_->window();
    case clock:
        return clock_panel_->window();
    case battery:
    case item_count:
        break;
    }
    return battery_panel_->window();
}

void Bar::toggle_panel(Item item) {
    close_panels_except(&panel_window(item));
    switch (item) {
    case tray:
        tray_panel_->toggle();
        break;
    case bluetooth:
        bluetooth_panel_->toggle();
        break;
    case network:
        network_panel_->toggle();
        break;
    case brightness:
        brightness_panel_->toggle();
        break;
    case volume:
        volume_panel_->toggle();
        break;
    case battery:
        battery_panel_->toggle();
        break;
    case media:
        media_panel_->toggle();
        break;
    case clock:
        clock_panel_->toggle();
        break;
    case item_count:
        break;
    }
}

void Bar::click(const xcb_button_press_event_t &event) {
    if (event.detail != XCB_BUTTON_INDEX_1) {
        return;
    }
    start_linger();
    if (std::optional<Item> item = item_at(event.event_x)) {
        toggle_panel(*item);
    } else {
        close_panels_except(nullptr);
        if (logout_rect_.contains(event.event_x)) {
            ipc_.dispatch("logout");
        } else if (workspace_rect_.contains(event.event_x)) {
            int offset = event.event_x - workspace_rect_.x - bar_config::pill_pad;
            if (workspace_overview_at(services_.i3.status(), offset)) {
                ipc_.dispatch("overview");
            } else if (auto index = workspace_at(services_.i3.status(), offset)) {
                services_.i3.switch_to(*index);
            }
        }
    }
}

void Bar::close_panels_except(const PanelWindow *keep) {
    for (std::size_t i = 0; i < item_count; ++i) {
        if (PanelWindow &panel = panel_window(static_cast<Item>(i)); &panel != keep) {
            panel.close();
        }
    }
}

std::optional<Bar::Item> Bar::open_item() {
    for (std::size_t i = 0; i < item_count; ++i) {
        if (panel_window(static_cast<Item>(i)).is_open()) {
            return static_cast<Item>(i);
        }
    }
    return std::nullopt;
}

std::optional<Bar::Item> Bar::item_at(int x) const {
    for (std::size_t i = 0; i < item_count; ++i) {
        const Rect &rect = item_rects_[i];
        if (items_[i]->visible() && x >= rect.x - bar_config::item_gap / 2 &&
            x < rect.x + rect.width + bar_config::item_gap / 2) {
            return static_cast<Item>(i);
        }
    }
    return std::nullopt;
}

bool Bar::pin_open_item() {
    std::optional<Item> open = open_item();
    bool changed = false;
    for (std::size_t i = 0; i < item_count; ++i) {
        changed |= items_[i]->set_pinned(open == static_cast<Item>(i));
    }
    return changed;
}

void Bar::sync_panels() {
    if (open_item()) {
        if (pin_open_item()) {
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
    std::optional<Item> hovered = x && !open_item() ? item_at(*x) : std::nullopt;
    bool changed = logout_->set_hovered(x && logout_rect_.contains(*x));
    for (std::size_t i = 0; i < item_count; ++i) {
        changed |= items_[i]->set_hovered(hovered == static_cast<Item>(i));
    }
    if (changed || redraw) {
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
    bool pinned = pin_open_item();
    hover(inside ? std::optional<int>(x) : std::nullopt, pinned);
}

std::chrono::milliseconds Bar::until_linger_end() const {
    auto left = linger_until_ - std::chrono::steady_clock::now();
    if (left < std::chrono::steady_clock::duration::zero()) {
        return std::chrono::hours(1);
    }
    return std::chrono::ceil<std::chrono::milliseconds>(left);
}

} // namespace astralia
