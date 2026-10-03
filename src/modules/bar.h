#pragma once

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
class BluetoothPanel;
class ClockWidget;
class ControlCenterPanel;
class ControlCenterWidget;
class LogoutWidget;
class NetworkPanel;
class PanelWindow;
class StatusWidget;
enum class StatusItem : std::size_t;

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

    void set_hints(const OutputGeometry &output);
    void paint_background(const Rect &rect);
    void paint_panel();
    void draw_divider(const Rect &left, const Rect &right);
    void draw_all();
    void draw_clock();
    void redraw_clock();
    void redraw_status();
    void notify(const std::string &app, const StatusMessage &message);
    void click(const xcb_button_press_event_t &event);
    void hover(std::optional<int> x, bool redraw = false);
    void close_panels_except(const PanelWindow *keep);
    std::optional<StatusItem> open_item() const;
    void sync_panels();
    void start_linger();
    void sync_hover();
    std::chrono::milliseconds until_linger_end() const;
    Rect clock_rect() const;

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
    std::unique_ptr<LogoutWidget> logout_;
    std::unique_ptr<StatusWidget> status_;
    std::unique_ptr<ControlCenterWidget> control_center_;
    std::unique_ptr<ControlCenterPanel> control_center_panel_;
    std::unique_ptr<AudioPanel> audio_panel_;
    std::unique_ptr<BatteryPanel> battery_panel_;
    std::unique_ptr<BluetoothPanel> bluetooth_panel_;
    std::unique_ptr<NetworkPanel> network_panel_;
    Rect clock_rect_{};
    Rect logout_rect_{};
    Rect workspace_rect_{};
    Rect status_rect_{};
    Rect control_center_rect_{};
    std::chrono::steady_clock::time_point linger_until_{};
    int linger_timer_ = -1;
};

} // namespace astralia
