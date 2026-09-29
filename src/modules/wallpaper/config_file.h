#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astralia {

struct WallpaperFile {
    std::unordered_map<std::string, std::string> images;
    std::vector<std::size_t> invalid_lines;
};

std::string wallpaper_file_path(std::string_view config_home, std::string_view home);
std::string expand_home(std::string_view path, std::string_view home);
WallpaperFile parse_wallpaper_file(std::string_view text, std::string_view home);
std::optional<std::string> image_for(const WallpaperFile &file, const std::string &output);

} // namespace astralia
