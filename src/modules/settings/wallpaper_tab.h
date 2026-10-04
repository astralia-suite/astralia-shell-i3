#pragma once

#include <cairo.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "app/services.h"

#include "core/event_loop.h"
#include "core/keyboard.h"

#include "modules/settings/thumbnail_cache.h"

#include "render/panel_chrome.h"

namespace astralia {

class WallpaperTab {
  public:
    WallpaperTab(Services &services, EventLoop &loop, std::function<void()> changed);

    void open();
    void close();
    void refresh();
    void paint(cairo_t *cr, const PanelRect &area, std::vector<PanelHit> &hits);
    bool click(const std::optional<PanelHit> &hit);
    bool key(const KeyEvent &event);
    bool scroll(int direction);

  private:
    enum Action { chip = 1,
                  pick,
                  folder_field,
                  reset };

    void scan();

    Services &services_;
    std::string selected_;
    std::string scanned_dir_;
    std::vector<std::string> images_;
    ThumbnailCache thumbnails_;
    PanelRect grid_;
    double scroll_ = 0.0;
    bool editing_ = false;
    std::string buffer_;
};

} // namespace astralia
