#include <algorithm>

#include "modules/wallpaper/image.h"

namespace astralia {

Placement cover(int image_width, int image_height, int area_width, int area_height) {
    double scale = std::max(static_cast<double>(area_width) / image_width,
                            static_cast<double>(area_height) / image_height);
    return {scale, (area_width - image_width * scale) / 2.0,
            (area_height - image_height * scale) / 2.0};
}

std::expected<SurfacePtr, std::string> load_image(const std::string &path) {
    return decode_image(path);
}

} // namespace astralia
