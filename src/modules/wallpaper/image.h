#pragma once

#include <expected>
#include <string>

#include "core/image_decode.h"

namespace astralia {

struct Placement {
    double scale;
    double x;
    double y;
};

Placement cover(int image_width, int image_height, int area_width, int area_height);
std::expected<SurfacePtr, std::string> load_image(const std::string &path);

} // namespace astralia
