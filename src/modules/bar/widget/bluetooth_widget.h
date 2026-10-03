#pragma once

#include <string>

#include "modules/bar/widget/bar_widget.h"

#include "service/bluetooth_service.h"

namespace astralia {

const char *bluetooth_icon(const BluetoothStatus &status);
std::string bluetooth_label(const BluetoothStatus &status);

class BluetoothWidget : public BarWidget {
  public:
    void update(const BluetoothStatus &status);
};

} // namespace astralia
