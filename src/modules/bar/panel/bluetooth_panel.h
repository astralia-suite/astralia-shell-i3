#pragma once

#include <string>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/panel_chrome.h"
#include "render/panel_window.h"

#include "service/bluetooth_service.h"

namespace astralia {

class BluetoothPanel {
  public:
    BluetoothPanel(XConnection &x, EventLoop &loop, BluetoothService &bluetooth);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { close_panel = 1,
                  power,
                  device,
                  forget,
                  cancel,
                  confirm };

    void handle(const xcb_generic_event_t &event);
    void click(int x, int y);
    void scroll(int delta);
    void close_confirm();
    void paint();

    BluetoothService &bluetooth_;
    PanelWindow window_;
    std::vector<PanelHit> hits_;
    int scroll_ = 0;
    int content_height_ = 0;
    int visible_height_ = 0;
    int main_height_ = 0;
    Action confirm_action_ = close_panel;
    std::string confirm_path_;
};

} // namespace astralia
