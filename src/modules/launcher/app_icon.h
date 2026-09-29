#pragma once

#include <string>
#include <vector>

#include "core/image_decode.h"

namespace astralia {

using IconSurface = SurfacePtr;

std::vector<std::string> icon_theme_order(const std::string &active);
std::string resolve_app_icon_path(const std::string &icon_field);
IconSurface load_app_icon(const std::string &path, int size);

} // namespace astralia
