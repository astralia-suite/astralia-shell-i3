#pragma once

#include "modules/bar/widget/bar_widget.h"

#include "service/network_service.h"

namespace astralia {

const char *network_icon(const NetworkStatus &status);

class NetworkWidget : public BarWidget {
  public:
    void update(const NetworkStatus &status);
};

} // namespace astralia
