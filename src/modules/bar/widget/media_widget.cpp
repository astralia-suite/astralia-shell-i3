#include "config/bar_config.h"

#include "modules/bar/widget/media_widget.h"

#include "render/icons.h"

namespace astralia {

MediaWidget::MediaWidget() {
    set_icon(icon::music_note);
    set_label(bar_config::media_label);
}

} // namespace astralia
