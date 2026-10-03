#pragma once

#include <array>
#include <cairo.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/x_connection.h"

#include "render/x_window.h"

namespace astralia {

class AudioPanel;
class BatteryPanel;
class BatteryWidget;
class BluetoothPanel;
class BluetoothWidget;
class BrightnessPanel;
class BrightnessWidget;
class ClockPanel;
class ClockWidget;
class LogoutWidget;
class MediaPanel;
class MediaWidget;
class NetworkPanel;
class NetworkWidget;
class PanelWindow;
class VolumeWidget;
class WidgetCapsule;

class Bar {
  public:
    Bar(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services);
    ~Bar();
    Bar(const Bar &) = delete;
    Bar &operator=(const Bar &) = delete;

  private:
    struct Rect {
        int x;
        int y;
        int width;
        int height;

        bool contains(int px) const { return px >= x && px < x + width; }
    };

    enum Item : std::size_t { network,
                              bluetooth,
                              volume,
                              brightness,
                              battery,
                              media,
                              clock,
                              item_count };

    void set_hints(const OutputGeometry &output);
    void paint_background(const Rect &rect);
    void paint_panel();
    void draw_divider(const Rect &left, const Rect &right);
    void draw_all();
    void redraw_clock();
    void refresh(Item item);
    void notify(const std::string &app, const StatusMessage &message);
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
    Rect panel_{};
    XWindow window_;
    std::unique_ptr<sdbus::IProxy> notifier_;
    std::unique_ptr<ClockWidget> clock_;
    std::unique_ptr<MediaWidget> media_;
    std::unique_ptr<LogoutWidget> logout_;
    std::unique_ptr<BluetoothWidget> bluetooth_;
    std::unique_ptr<NetworkWidget> network_;
    std::unique_ptr<BrightnessWidget> brightness_;
    std::unique_ptr<VolumeWidget> volume_;
    std::unique_ptr<BatteryWidget> battery_;
    std::array<WidgetCapsule *, item_count> items_{};
    std::array<Rect, item_count> item_rects_{};
    std::unique_ptr<AudioPanel> audio_panel_;
    std::unique_ptr<BatteryPanel> battery_panel_;
    std::unique_ptr<BluetoothPanel> bluetooth_panel_;
    std::unique_ptr<BrightnessPanel> brightness_panel_;
    std::unique_ptr<NetworkPanel> network_panel_;
    std::unique_ptr<MediaPanel> media_panel_;
    std::unique_ptr<ClockPanel> clock_panel_;
    Rect logout_rect_{};
    Rect workspace_rect_{};
    std::chrono::steady_clock::time_point linger_until_{};
    int linger_timer_ = -1;
};

} // namespace astralia
