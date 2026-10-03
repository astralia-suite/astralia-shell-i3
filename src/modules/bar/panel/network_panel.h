#pragma once

#include <string>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/image_decode.h"
#include "render/panel_chrome.h"
#include "render/panel_window.h"

#include "service/network_service.h"

namespace astralia {

const char *network_signal_icon(int percent);

class NetworkPanel {
  public:
    NetworkPanel(XConnection &x, EventLoop &loop, NetworkService &network);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { close_panel = 1,
                  wifi,
                  rescan,
                  dismiss_error,
                  network,
                  forget,
                  cancel,
                  confirm };

    enum class Sub { none,
                     password,
                     disconnect,
                     forget };

    void handle(const xcb_generic_event_t &event);
    void key(const xcb_key_press_event_t &event);
    void click(int x, int y);
    void scroll(int delta);
    void open_sub(Sub sub, const std::string &ssid);
    void close_sub();
    void submit();
    void paint();

    NetworkService &network_;
    PanelWindow window_;
    std::vector<PanelHit> hits_;
    int scroll_ = 0;
    int content_height_ = 0;
    int visible_height_ = 0;
    int main_height_ = 0;
    Sub sub_ = Sub::none;
    std::string sub_ssid_;
    std::string password_;
    SurfacePtr echo_;
};

} // namespace astralia
