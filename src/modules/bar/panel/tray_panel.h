#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>

#include "core/event_loop.h"
#include "core/x_connection.h"

#include "render/app_icon.h"
#include "render/panel_chrome.h"
#include "render/panel_window.h"
#include "render/x_window.h"

#include "service/tray_service.h"

namespace astralia {

int tray_menu_height(const std::vector<TrayMenuEntry> *level, bool show_back);

class TrayMenu {
  public:
    TrayMenu(XConnection &x, EventLoop &loop, TrayService &tray);

    bool is_open() const { return open_; }
    void open(const std::string &key, int x, int y);
    void close();
    void back();
    void refresh();

  private:
    enum Action { go_back = 1,
                  menu_entry };

    void handle(const xcb_generic_event_t &event);
    void click(double x, double y);
    void hover(std::optional<double> y);
    void paint();
    const std::vector<TrayMenuEntry> *level() const;

    XConnection &x_;
    TrayService &tray_;
    XWindow window_;
    bool open_ = false;
    std::string key_;
    std::vector<int32_t> path_;
    int anchor_x_ = 0;
    int anchor_y_ = 0;
    int hovered_ = -1;
    std::vector<PanelHit> hits_;
};

class TrayPanel {
  public:
    TrayPanel(XConnection &x, EventLoop &loop, TrayService &tray);

    void toggle();
    PanelWindow &window() { return window_; }

  private:
    enum Action { close_panel = 1,
                  item };

    void handle(const xcb_generic_event_t &event);
    void click(double x, double y, uint8_t button);
    void paint();
    cairo_surface_t *icon(const TrayItem &item);

    TrayService &tray_;
    TrayMenu menu_;
    PanelWindow window_;
    std::vector<PanelHit> hits_;
    std::unordered_map<std::string, IconSurface> icons_;
};

} // namespace astralia
