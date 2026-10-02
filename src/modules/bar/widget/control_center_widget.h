#pragma once

#include <cairo.h>

#include "core/text.h"

namespace astralia {

class ControlCenterWidget {
  public:
    ControlCenterWidget();

    int width() const;
    void draw(cairo_t *cr, double x, int top, int height) const;

  private:
    Text icon_;
};

} // namespace astralia
