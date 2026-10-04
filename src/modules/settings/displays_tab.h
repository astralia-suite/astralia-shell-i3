#pragma once

#include <cairo.h>
#include <optional>
#include <string>
#include <vector>

#include "app/services.h"

#include "render/panel_chrome.h"

namespace astralia {

class DisplaysTab {
  public:
    explicit DisplaysTab(Services &services) : services_(services) {}

    void paint(cairo_t *cr, const PanelRect &area, std::vector<PanelHit> &hits);
    bool click(const std::optional<PanelHit> &hit);

  private:
    enum Action { chip = 1,
                  override_toggle,
                  feature_toggle };

    Services &services_;
    std::string selected_;
};

} // namespace astralia
