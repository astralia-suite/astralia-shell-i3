#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <jpeglib.h>
#include <resvg/resvg.h>
#include <string>
#include <string_view>
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

struct CloseFile {
    void operator()(std::FILE *file) const { std::fclose(file); }
};

struct JpegError {
    jpeg_error_mgr manager;
    std::jmp_buf jump;
};

using RequiredScale = std::function<double(int, int)>;

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

SurfacePtr adopt_pixels(std::unique_ptr<stbi_uc, FreePixels> pixels, int width, int height) {
    auto *words = reinterpret_cast<uint32_t *>(pixels.get());
    const uint8_t *in = pixels.get();
    for (std::size_t i = 0, count = static_cast<std::size_t>(width) * height; i < count; ++i, in += 4) {
        uint8_t alpha = in[3];
        words[i] = static_cast<uint32_t>(alpha) << 24 | premultiply(in[0], alpha) << 16 |
                   premultiply(in[1], alpha) << 8 | premultiply(in[2], alpha);
    }
    SurfacePtr surface(cairo_image_surface_create_for_data(pixels.get(), CAIRO_FORMAT_ARGB32, width,
                                                           height, width * 4));
    static cairo_user_data_key_t key;
    if (cairo_surface_set_user_data(surface.get(), &key, pixels.get(), [](void *data) { stbi_image_free(data); }) == CAIRO_STATUS_SUCCESS) {
        pixels.release();
    }
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

bool is_jpeg(const std::string &path) {
    constexpr std::array<std::string_view, 3> extensions{".jpg", ".jpeg", ".jfif"};
    std::string extension = std::filesystem::path(path).extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::ranges::find(extensions, extension) != extensions.end();
}

std::expected<SurfacePtr, std::string> decode_jpeg(const std::string &path, const RequiredScale &required_scale) {
    std::unique_ptr<std::FILE, CloseFile> file(std::fopen(path.c_str(), "rb"));
    if (!file) {
        return std::unexpected(std::string("cannot open file"));
    }
    jpeg_decompress_struct decoder;
    JpegError error;
    decoder.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = [](j_common_ptr info) { std::longjmp(reinterpret_cast<JpegError *>(info->err)->jump, 1); };
    error.manager.output_message = [](j_common_ptr) {};
    cairo_surface_t *volatile surface = nullptr;
    if (setjmp(error.jump) != 0) {
        jpeg_destroy_decompress(&decoder);
        cairo_surface_destroy(surface);
        return std::unexpected(std::string("jpeg decode failed"));
    }
    jpeg_create_decompress(&decoder);
    jpeg_stdio_src(&decoder, file.get());
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = JCS_EXT_BGRA;
    decoder.scale_num = 1;
    decoder.scale_denom = static_cast<unsigned>(jpeg_reduction(required_scale(static_cast<int>(decoder.image_width), static_cast<int>(decoder.image_height))));
    jpeg_start_decompress(&decoder);
    auto width = static_cast<int>(decoder.output_width);
    auto height = static_cast<int>(decoder.output_height);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        jpeg_destroy_decompress(&decoder);
        cairo_surface_destroy(surface);
        return std::unexpected(std::string("cannot allocate surface"));
    }
    uint8_t *pixels = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW row = pixels + static_cast<std::size_t>(decoder.output_scanline) * stride;
        jpeg_read_scanlines(&decoder, &row, 1);
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    cairo_surface_mark_dirty(surface);
    return SurfacePtr(surface);
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

Placement cover(int image_width, int image_height, int area_width, int area_height) {
    double scale = std::max(static_cast<double>(area_width) / image_width,
                            static_cast<double>(area_height) / image_height);
    return {scale, (area_width - image_width * scale) / 2.0,
            (area_height - image_height * scale) / 2.0};
}

int jpeg_reduction(double required_scale) {
    for (int reduction : {8, 4, 2}) {
        if (1.0 / reduction >= required_scale) {
            return reduction;
        }
    }
    return 1;
}

std::expected<SurfacePtr, std::string> decode_image(const std::string &path, int fit_size) {
    if (path.ends_with(".svg")) {
        return decode_svg(path, fit_size);
    }
    if (is_jpeg(path)) {
        auto image = decode_jpeg(path, [fit_size](int width, int height) { return fit_scale(width, height, fit_size); });
        if (!image) {
            return image;
        }
        return scaled(std::move(*image), fit_size);
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    std::unique_ptr<stbi_uc, FreePixels> pixels(
        stbi_load(path.c_str(), &width, &height, &channels, 4));
    if (!pixels) {
        return std::unexpected(std::string(stbi_failure_reason()));
    }
    return scaled(adopt_pixels(std::move(pixels), width, height), fit_size);
}

std::expected<SurfacePtr, std::string> decode_cover(const std::string &path, int width, int height) {
    std::expected<SurfacePtr, std::string> source;
    if (is_jpeg(path)) {
        source = decode_jpeg(path, [width, height](int image_width, int image_height) { return cover(image_width, image_height, width, height).scale; });
    } else if (path.ends_with(".svg")) {
        source = decode_svg(path, 2 * std::max(width, height));
    } else {
        source = decode_image(path);
    }
    if (!source) {
        return source;
    }
    cairo_surface_t *image = source->get();
    Placement placement = cover(cairo_image_surface_get_width(image), cairo_image_surface_get_height(image), width, height);
    SurfacePtr target(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
    cairo_t *cr = cairo_create(target.get());
    cairo_translate(cr, placement.x, placement.y);
    cairo_scale(cr, placement.scale, placement.scale);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_destroy(cr);
    return target;
}

bool surface_opaque(cairo_surface_t *surface) {
    cairo_surface_flush(surface);
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    const uint8_t *pixels = cairo_image_surface_get_data(surface);
    for (int y = 0; y < height; ++y) {
        const auto *row = reinterpret_cast<const uint32_t *>(pixels + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            if (row[x] >> 24 != 0xff) {
                return false;
            }
        }
    }
    return true;
}

bool write_jpeg(cairo_surface_t *surface, const char *path, int quality) {
    std::unique_ptr<std::FILE, CloseFile> file(std::fopen(path, "wb"));
    if (!file) {
        return false;
    }
    cairo_surface_flush(surface);
    jpeg_compress_struct encoder;
    JpegError error;
    encoder.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = [](j_common_ptr info) { std::longjmp(reinterpret_cast<JpegError *>(info->err)->jump, 1); };
    error.manager.output_message = [](j_common_ptr) {};
    if (setjmp(error.jump) != 0) {
        jpeg_destroy_compress(&encoder);
        return false;
    }
    jpeg_create_compress(&encoder);
    jpeg_stdio_dest(&encoder, file.get());
    encoder.image_width = static_cast<JDIMENSION>(cairo_image_surface_get_width(surface));
    encoder.image_height = static_cast<JDIMENSION>(cairo_image_surface_get_height(surface));
    encoder.input_components = 4;
    encoder.in_color_space = JCS_EXT_BGRX;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, quality, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    uint8_t *pixels = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    while (encoder.next_scanline < encoder.image_height) {
        JSAMPROW row = pixels + static_cast<std::size_t>(encoder.next_scanline) * stride;
        jpeg_write_scanlines(&encoder, &row, 1);
    }
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);
    return std::fflush(file.get()) == 0;
}

} // namespace astralia
