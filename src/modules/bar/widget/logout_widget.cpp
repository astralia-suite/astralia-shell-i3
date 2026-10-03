#include "config/bar_config.h"

#include "modules/bar/widget/logout_widget.h"

#include "render/icons.h"

namespace astralia {

LogoutWidget::LogoutWidget() {
    set_icon(icon::power);
    set_label(bar_config::logout_label);
}

} // namespace astralia
