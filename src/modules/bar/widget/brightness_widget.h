#pragma once

#include "modules/bar/widget/bar_widget.h"

#include "service/brightness_service.h"

namespace astralia {

class BrightnessWidget : public BarWidget {
  public:
    void update(const BrightnessService &brightness);
};

} // namespace astralia
