#include <format>

#include "modules/bar/widget/volume_widget.h"

#include "render/icons.h"

namespace astralia {

void VolumeWidget::update(const AudioLevel &volume) {
    set_visible(volume.present);
    set_icon(icon::volume_threshold(volume.muted, volume.percent));
    set_label(volume.muted ? "Muted" : std::format("{}%", volume.percent));
}

} // namespace astralia
