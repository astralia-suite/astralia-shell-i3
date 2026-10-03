#pragma once

#include <cairo.h>

#include "render/text.h"

namespace astralia {

class LogoutWidget {
  public:
    LogoutWidget();

    bool hover(bool hovered);
    int width() const;
    void draw(cairo_t *cr, double x, int top, int height) const;

  private:
    Text icon_;
    Text label_;
    bool hovered_ = false;
};

} // namespace astralia
