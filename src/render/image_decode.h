#pragma once

#include <cairo.h>
#include <expected>
#include <memory>
#include <string>

namespace astralia {

struct DestroySurface {
    void operator()(cairo_surface_t *surface) const { cairo_surface_destroy(surface); }
};

using SurfacePtr = std::unique_ptr<cairo_surface_t, DestroySurface>;

struct Placement {
    double scale;
    double x;
    double y;
};

Placement cover(int image_width, int image_height, int area_width, int area_height);
int jpeg_reduction(double required_scale);
std::expected<SurfacePtr, std::string> decode_image(const std::string &path, int fit_size = 0);
std::expected<SurfacePtr, std::string> decode_cover(const std::string &path, int width, int height);
bool surface_opaque(cairo_surface_t *surface);
bool write_jpeg(cairo_surface_t *surface, const char *path, int quality);

} // namespace astralia
