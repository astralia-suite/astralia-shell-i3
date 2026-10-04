#include "config/bar_config.h"

#include "modules/bar/widget/tray_widget.h"

#include "render/icons.h"

namespace astralia {

TrayWidget::TrayWidget() {
    set_icon(icon::apps);
    set_label(bar_config::tray_label);
}

} // namespace astralia
