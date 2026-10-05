#pragma once

#include <cairo.h>
#include <optional>
#include <vector>

#include "app/services.h"

#include "render/panel_chrome.h"

namespace astralia {

class BarTab {
  public:
    explicit BarTab(Services &services) : services_(services) {}

    void paint(cairo_t *cr, const PanelRect &area, std::vector<PanelHit> &hits);
    bool click(const std::optional<PanelHit> &hit);

  private:
    enum Action { style = 1 };

    Services &services_;
};

} // namespace astralia
