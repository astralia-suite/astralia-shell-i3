#pragma once

#include <chrono>

#include "modules/bar/widget/bar_widget.h"

namespace astralia {

std::chrono::milliseconds ms_until_next_second(std::chrono::system_clock::time_point now);

class ClockWidget : public BarWidget {
  public:
    ClockWidget();

    bool refresh();
};

} // namespace astralia
