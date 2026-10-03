#pragma once

#include <string>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/panel_chrome.h"
#include "render/panel_window.h"

#include "service/battery_service.h"

namespace astralia {

std::string battery_time_left(int seconds);
std::string battery_state_label(const BatteryStatus &status);

class BatteryPanel {
  public:
    BatteryPanel(XConnection &x, EventLoop &loop, BatteryService &battery);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { close_panel = 1 };

    void handle(const xcb_generic_event_t &event);
    void paint();

    BatteryService &battery_;
    PanelWindow window_;
    std::vector<PanelHit> hits_;
};

} // namespace astralia
