#include <algorithm>
#include <cmath>
#include <cstdint>
#include <resvg/resvg.h>
#include <string>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include <stb/stb_image.h>

#include "render/image_decode.h"

namespace astralia {

namespace {

struct FreePixels {
    void operator()(stbi_uc *pixels) const { stbi_image_free(pixels); }
};

struct DestroyTree {
    void operator()(resvg_render_tree *tree) const { resvg_tree_destroy(tree); }
};

uint32_t premultiply(uint8_t channel, uint8_t alpha) { return (channel * alpha + 127) / 255; }

SurfacePtr to_surface(const uint8_t *rgba, int width, int height, bool premultiplied) {
    SurfacePtr surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
    cairo_surface_flush(surface.get());
    int stride = cairo_image_surface_get_stride(surface.get());
    uint8_t *target = cairo_image_surface_get_data(surface.get());
    for (int y = 0; y < height; ++y) {
        const uint8_t *in = rgba + static_cast<std::size_t>(y) * width * 4;
        auto *out = reinterpret_cast<uint32_t *>(target + y * stride);
        for (int x = 0; x < width; ++x, in += 4) {
            uint8_t alpha = in[3];
            uint32_t r = premultiplied ? in[0] : premultiply(in[0], alpha);
            uint32_t g = premultiplied ? in[1] : premultiply(in[1], alpha);
            uint32_t b = premultiplied ? in[2] : premultiply(in[2], alpha);
            out[x] = static_cast<uint32_t>(alpha) << 24 | r << 16 | g << 8 | b;
        }
    }
    cairo_surface_mark_dirty(surface.get());
    return surface;
}

double fit_scale(double width, double height, int fit_size) {
    return fit_size > 0 ? std::min(fit_size / width, fit_size / height) : 1.0;
}

SurfacePtr scaled(SurfacePtr source, int fit_size) {
    int width = cairo_image_surface_get_width(source.get());
    int height = cairo_image_surface_get_height(source.get());
    double scale = fit_scale(width, height, fit_size);
    if (scale == 1.0) {
        return source;
    }
    int target_width = std::max(1, static_cast<int>(std::lround(width * scale)));
    int target_height = std::max(1, static_cast<int>(std::lround(height * scale)));
    SurfacePtr target(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_width, target_height));
    cairo_t *cr = cairo_create(target.get());
    cairo_scale(cr, static_cast<double>(target_width) / width,
                static_cast<double>(target_height) / height);
    cairo_set_source_surface(cr, source.get(), 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_destroy(cr);
    return target;
}

std::expected<SurfacePtr, std::string> decode_svg(const std::string &path, int fit_size) {
    static resvg_options *options = resvg_options_create();
    resvg_render_tree *raw = nullptr;
    if (int32_t status = resvg_parse_tree_from_file(path.c_str(), options, &raw);
        status != RESVG_OK) {
        return std::unexpected("resvg error " + std::to_string(status));
    }
    std::unique_ptr<resvg_render_tree, DestroyTree> tree(raw);
    resvg_size size = resvg_get_image_size(tree.get());
    if (size.width <= 0 || size.height <= 0) {
        return std::unexpected(std::string("empty svg"));
    }
    double scale = fit_scale(size.width, size.height, fit_size);
    int width = std::max(1, static_cast<int>(std::lround(size.width * scale)));
    int height = std::max(1, static_cast<int>(std::lround(size.height * scale)));
    std::vector<uint8_t> rgba(static_cast<std::size_t>(width) * height * 4);
    resvg_transform transform{static_cast<float>(scale), 0, 0, static_cast<float>(scale), 0, 0};
    resvg_render(tree.get(), transform, width, height, reinterpret_cast<char *>(rgba.data()));
    return to_surface(rgba.data(), width, height, true);
}

} // namespace

std::expected<SurfacePtr, std::string> decode_image(const std::string &path, int fit_size) {
    if (path.ends_with(".svg")) {
        return decode_svg(path, fit_size);
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    std::unique_ptr<stbi_uc, FreePixels> pixels(
        stbi_load(path.c_str(), &width, &height, &channels, 4));
    if (!pixels) {
        return std::unexpected(std::string(stbi_failure_reason()));
    }
    return scaled(to_surface(pixels.get(), width, height, false), fit_size);
}

} // namespace astralia
