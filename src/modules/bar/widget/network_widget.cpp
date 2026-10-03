#include "modules/bar/widget/network_widget.h"

#include "render/icons.h"

namespace astralia {

const char *network_icon(const NetworkStatus &status) {
    if (status.portal) {
        return icon::lock;
    }
    switch (status.kind) {
    case NetworkKind::ethernet:
        return icon::router;
    case NetworkKind::wifi:
        if (status.strength > 75) {
            return icon::wifi;
        }
        if (status.strength > 50) {
            return icon::wifi2;
        }
        if (status.strength > 25) {
            return icon::wifi1;
        }
        return icon::wifi0;
    case NetworkKind::none:
        break;
    }
    return icon::wifi_off;
}

void NetworkWidget::update(const NetworkStatus &status) {
    set_icon(network_icon(status));
    set_label(status.kind == NetworkKind::wifi ? status.ssid : "");
}

} // namespace astralia
