#pragma once

#include <cstddef>
#include <vector>
#include <xcb/xcb.h>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/keyboard.h"
#include "core/x_connection.h"

#include "modules/settings/displays_tab.h"
#include "modules/settings/layout.h"
#include "modules/settings/wallpaper_tab.h"

#include "render/app_fonts.h"
#include "render/panel_chrome.h"
#include "render/x_window.h"

namespace astralia {

class Settings {
  public:
    Settings(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services);

  private:
    enum Tab : std::size_t { displays,
                             wallpaper };

    void toggle();
    void open();
    void close();
    void select(Tab tab);
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void click(double x, double y);
    void scroll(int direction);
    void sync();
    void paint();
    void grab();
    void release_grab();

    XConnection &x_;
    Services &services_;
    XWindow window_;
    Keyboard keyboard_;
    bool fonts_ = register_app_fonts();
    DisplaysTab displays_;
    WallpaperTab wallpaper_;
    Tab tab_ = displays;
    SettingsGeometry geometry_{};
    std::vector<PanelHit> hits_;
    bool open_ = false;
    bool grabbed_ = false;
};

} // namespace astralia
