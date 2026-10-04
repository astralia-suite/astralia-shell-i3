#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/config_file.h"
#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

struct WallpaperFile {
    std::unordered_map<std::string, std::string> images;
    std::vector<std::size_t> invalid_lines;

    bool operator==(const WallpaperFile &) const = default;
};

WallpaperFile parse_wallpaper_file(std::string_view text, std::string_view home);
std::optional<std::string> image_for(const WallpaperFile &file, const std::string &output);

class WallpaperService {
  public:
    explicit WallpaperService(EventLoop &loop);

    std::optional<std::string> image_for(const std::string &output) const;
    std::optional<std::string> own_image(const std::string &output) const;
    void set_image(const std::string &output, const std::string &path);
    void clear_image(const std::string &output);

    Signal<> changed;

  private:
    void store(std::string text);
    void reload();

    std::string path_;
    std::string home_;
    std::string text_;
    WallpaperFile file_;
    FileWatch watch_;
};

} // namespace astralia
