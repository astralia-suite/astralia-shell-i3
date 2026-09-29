#include <format>

#include "config/wallpaper_config.h"

#include "modules/wallpaper/config_file.h"

namespace astralia {

namespace {

std::string_view trim(std::string_view text) {
    constexpr std::string_view space = " \t\r";
    std::size_t first = text.find_first_not_of(space);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

} // namespace

std::string wallpaper_file_path(std::string_view config_home, std::string_view home) {
    if (!config_home.empty()) {
        return std::format("{}/{}", config_home, wallpaper_config::file);
    }
    return std::format("{}/.config/{}", home, wallpaper_config::file);
}

std::string expand_home(std::string_view path, std::string_view home) {
    if (path == "~") {
        return std::string(home);
    }
    if (path.starts_with("~/")) {
        return std::format("{}{}", home, path.substr(1));
    }
    return std::string(path);
}

WallpaperFile parse_wallpaper_file(std::string_view text, std::string_view home) {
    WallpaperFile file;
    std::size_t number = 0;
    while (!text.empty()) {
        ++number;
        std::size_t end = text.find('\n');
        std::string_view line = trim(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::size_t equals = line.find('=');
        std::string_view output = trim(line.substr(0, equals));
        std::string_view image =
            equals == std::string_view::npos ? std::string_view{} : trim(line.substr(equals + 1));
        if (output.empty() || image.empty()) {
            file.invalid_lines.push_back(number);
            continue;
        }
        file.images.insert_or_assign(std::string(output), expand_home(image, home));
    }
    return file;
}

std::optional<std::string> image_for(const WallpaperFile &file, const std::string &output) {
    if (auto it = file.images.find(output); it != file.images.end()) {
        return it->second;
    }
    if (auto it = file.images.find(wallpaper_config::any_output); it != file.images.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace astralia
