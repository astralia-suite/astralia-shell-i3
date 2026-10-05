#pragma once

#include "config/bar_config.h"

#include "modules/bar/widget/widget_capsule.h"

namespace astralia {

class BarWidget : public WidgetCapsule {
  public:
    BarWidget() : WidgetCapsule({bar_config::icon_font, bar_config::font, bar_config::label_gap}) {}
};

} // namespace astralia
