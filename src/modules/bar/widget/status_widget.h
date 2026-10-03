#pragma once

#include <array>
#include <cairo.h>
#include <cstddef>
#include <optional>
#include <string>

#include "render/text.h"

#include "service/audio_service.h"
#include "service/battery_service.h"
#include "service/bluetooth_service.h"
#include "service/network_service.h"

namespace astralia {

const char *bluetooth_icon(const BluetoothStatus &status);
std::string bluetooth_label(const BluetoothStatus &status);
const char *network_icon(const NetworkStatus &status);
const char *battery_icon(const BatteryStatus &status);
std::string battery_label(const BatteryStatus &status);

enum class StatusItem : std::size_t { bluetooth,
                                      network,
                                      volume,
                                      battery };

class StatusWidget {
  public:
    void update(const BluetoothStatus &bluetooth, const NetworkStatus &network,
                const AudioLevel &volume, const BatteryStatus &battery);
    std::optional<StatusItem> item_at(int offset) const;
    bool hover(std::optional<int> offset);
    int width() const;
    void draw(cairo_t *cr, double x, int top, int height) const;

  private:
    struct Item {
        Text icon;
        Text label;
        bool visible = true;

        Item();
        int width(bool show_label) const;
    };

    std::array<Item, 4> items_;
    std::optional<std::size_t> hovered_;
};

} // namespace astralia
