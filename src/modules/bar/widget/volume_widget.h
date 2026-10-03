#pragma once

#include "modules/bar/widget/bar_widget.h"

#include "service/audio_service.h"

namespace astralia {

class VolumeWidget : public BarWidget {
  public:
    void update(const AudioLevel &volume);
};

} // namespace astralia
