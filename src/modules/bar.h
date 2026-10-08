#pragma once

#include <array>
#include <cairo.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/x_connection.h"

#include "modules/bar/styles/geometry.h"

#include "render/x_window.h"

namespace astralia {

class BatteryPanel;
class BatteryWidget;
class BluetoothPanel;
class BluetoothWidget;
class BrightnessPanel;
class BrightnessWidget;
class ClockPanel;
class ClockWidget;
class DockWidget;
class LogoutWidget;
class MediaPanel;
class MediaWidget;
class NetworkPanel;
class NetworkWidget;
class PanelWindow;
class TrayPanel;
class TrayWidget;
class VolumePanel;
class VolumeWidget;
class WidgetCapsule;

class Bar {
  public:
    Bar(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services, const Output &output);
    ~Bar();
    Bar(const Bar &) = delete;
    Bar &operator=(const Bar &) = delete;

    void place(const Output &output);
    void hide();

  private:
    using Rect = BarRect;

    enum Item : std::size_t { tray,
                              network,
                              bluetooth,
                              volume,
                              brightness,
                              battery,
                              media,
                              clock,
                              item_count };

    void apply_output(const OutputGeometry &output);
    void set_hints(const OutputGeometry &output);
    bool refresh_style();
    void apply_panel_geometry(const OutputGeometry &output);
    void paint_background(const Rect &rect);
    void paint_frame();
    void layout();
    void add_divider(const Rect &left, const Rect &right);
    void draw_divider(double x);
    void draw_all();
    void redraw_clock();
    void refresh_dock();
    void refresh(Item item);
    void click(const xcb_button_press_event_t &event);
    void hover(std::optional<int> x, bool redraw = false);
    void close_panels_except(const PanelWindow *keep);
    PanelWindow &panel_window(Item item);
    void toggle_panel(Item item);
    std::optional<Item> open_item();
    std::optional<Item> item_at(int x) const;
    bool pin_open_item();
    void sync_panels();
    void start_linger();
    void sync_hover();
    std::chrono::milliseconds until_linger_end() const;

    XConnection &x_;
    EventLoop &loop_;
    IpcServer &ipc_;
    Services &services_;
    uint16_t width_;
    uint16_t height_;
    BarStyle style_ = BarStyle::continuous;
    const BarStyleSpec *spec_ = nullptr;
    Rect panel_{};
    std::vector<double> dividers_;
    std::optional<double> left_end_;
    std::optional<IslandSpan> center_span_;
    std::optional<double> right_start_;
    XWindow window_;
    std::unique_ptr<ClockWidget> clock_;
    std::unique_ptr<MediaWidget> media_;
    std::unique_ptr<LogoutWidget> logout_;
    std::unique_ptr<DockWidget> dock_;
    std::unique_ptr<BluetoothWidget> bluetooth_;
    std::unique_ptr<NetworkWidget> network_;
    std::unique_ptr<BrightnessWidget> brightness_;
    std::unique_ptr<VolumeWidget> volume_;
    std::unique_ptr<BatteryWidget> battery_;
    std::unique_ptr<TrayWidget> tray_;
    std::array<WidgetCapsule *, item_count> items_{};
    std::array<Rect, item_count> item_rects_{};
    std::unique_ptr<VolumePanel> volume_panel_;
    std::unique_ptr<BatteryPanel> battery_panel_;
    std::unique_ptr<BluetoothPanel> bluetooth_panel_;
    std::unique_ptr<BrightnessPanel> brightness_panel_;
    std::unique_ptr<NetworkPanel> network_panel_;
    std::unique_ptr<MediaPanel> media_panel_;
    std::unique_ptr<ClockPanel> clock_panel_;
    std::unique_ptr<TrayPanel> tray_panel_;
    Rect logout_rect_{};
    Rect workspace_rect_{};
    Rect dock_rect_{};
    std::chrono::steady_clock::time_point linger_until_{};
    int linger_timer_ = -1;
};

} // namespace astralia
