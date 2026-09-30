#pragma once

#include <cairo.h>
#include <chrono>

#include "core/text.h"

namespace astralia {

std::chrono::milliseconds ms_until_next_second(std::chrono::system_clock::time_point now);

class ClockWidget {
  public:
    ClockWidget();

    bool refresh();
    int width() const { return text_.width(); }
    void draw(cairo_t *cr, double cx, double cy) const { text_.draw_ink_centered(cr, cx, cy); }

  private:
    Text text_;
};

} // namespace astralia
