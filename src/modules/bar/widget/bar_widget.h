#pragma once

#include "config/bar_config.h"

#include "render/widget_capsule.h"

namespace astralia {

class BarWidget : public WidgetCapsule {
  public:
    explicit BarWidget(LabelMode mode = LabelMode::on_hover)
        : WidgetCapsule({bar_config::icon_font, bar_config::font, bar_config::label_gap}, mode) {}
};

} // namespace astralia
