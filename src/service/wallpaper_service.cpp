#include <cstdlib>
#include <utility>

#include "config/wallpaper_config.h"

#include "core/log.h"

#include "service/wallpaper_service.h"

namespace astralia {

namespace {

std::string home_dir() {
    const char *home = std::getenv("HOME");
    return home != nullptr ? home : "";
}

std::string initial_text(const std::string &path) {
    std::optional<std::string> text = read_text_file(path);
    if (!text) {
        log::info("no wallpaper config at {}", path);
    }
    return text.value_or("");
}

} // namespace

WallpaperFile parse_wallpaper_file(std::string_view text, std::string_view home) {
    WallpaperFile file;
    ConfigLines lines = parse_config_lines(text);
    file.invalid_lines = std::move(lines.invalid);
    for (const ConfigEntry &entry : lines.entries) {
        file.images.insert_or_assign(entry.key, expand_home(entry.value, home));
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

WallpaperService::WallpaperService(EventLoop &loop)
    : path_(user_config_path(wallpaper_config::file)), home_(home_dir()),
      text_(initial_text(path_)), file_(parse_wallpaper_file(text_, home_)),
      watch_(loop, path_, [this] { reload(); }) {
    for (std::size_t line : file_.invalid_lines) {
        log::error("{}:{}: expected `output = image`", path_, line);
    }
}

std::optional<std::string> WallpaperService::image_for(const std::string &output) const {
    return astralia::image_for(file_, output);
}

std::optional<std::string> WallpaperService::own_image(const std::string &output) const {
    if (auto it = file_.images.find(output); it != file_.images.end()) {
        return it->second;
    }
    return std::nullopt;
}

void WallpaperService::set_image(const std::string &output, const std::string &path) {
    store(with_entry(text_, output, path));
}

void WallpaperService::clear_image(const std::string &output) {
    store(without_entry(text_, output));
}

void WallpaperService::store(std::string text) {
    if (!write_text_file(path_, text)) {
        return;
    }
    text_ = std::move(text);
    WallpaperFile next = parse_wallpaper_file(text_, home_);
    if (next == file_) {
        return;
    }
    file_ = std::move(next);
    changed.emit();
}

void WallpaperService::reload() {
    std::string text = read_text_file(path_).value_or("");
    if (text == text_) {
        return;
    }
    log::info("reloading {}", path_);
    text_ = std::move(text);
    WallpaperFile next = parse_wallpaper_file(text_, home_);
    for (std::size_t line : next.invalid_lines) {
        log::error("{}:{}: expected `output = image`", path_, line);
    }
    if (next == file_) {
        return;
    }
    file_ = std::move(next);
    changed.emit();
}

} // namespace astralia
