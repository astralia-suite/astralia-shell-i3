#pragma once

#include <cairo.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/app_icon.h"

#include "service/i3_service.h"

namespace astralia {

struct DockEntry {
    std::string window_class;
    bool focused = false;

    bool operator==(const DockEntry &) const = default;
};

std::vector<DockEntry> dock_entries(const I3Tree &tree, uint32_t workspace);

class DockWidget {
  public:
    bool update(const I3Tree &tree, uint32_t workspace);
    bool visible() const { return !entries_.empty(); }
    int width() const;
    void draw(cairo_t *cr, double x, int top, int height);

  private:
    cairo_surface_t *icon_for(const std::string &window_class);

    std::vector<DockEntry> entries_;
    std::unordered_map<std::string, IconSurface> icons_;
};

} // namespace astralia
