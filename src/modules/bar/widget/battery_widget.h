#pragma once

#include <string>

#include "modules/bar/widget/bar_widget.h"

#include "service/battery_service.h"

namespace astralia {

const char *battery_icon(const BatteryStatus &status);
std::string battery_label(const BatteryStatus &status);

class BatteryWidget : public BarWidget {
  public:
    void update(const BatteryStatus &status);
};

} // namespace astralia
