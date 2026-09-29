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
    int height() const { return text_.height(); }
    void draw(cairo_t *cr, double x, double y) const { text_.draw(cr, x, y); }

  private:
    Text text_;
};

} // namespace astralia
