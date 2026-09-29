#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>

#include "modules/launcher/app_icon.h"

namespace astralia {

namespace {

namespace fs = std::filesystem;

struct ThemeIndex {
    std::unordered_map<std::string, std::string> png;
    std::unordered_map<std::string, std::string> svg;
};

std::string home_dir() {
    const char *home = getenv("HOME");
    return home != nullptr ? home : "";
}

std::string active_gtk_theme() {
    std::string home = home_dir();
    if (home.empty()) {
        return "";
    }
    std::ifstream file(home + "/.config/gtk-3.0/settings.ini");
    constexpr std::string_view key = "gtk-icon-theme-name=";
    std::string line;
    while (std::getline(file, line)) {
        if (!line.starts_with(key)) {
            continue;
        }
        std::string value = line.substr(key.size());
        while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) {
            value.pop_back();
        }
        return value;
    }
    return "";
}

void index_root(const std::string &root, ThemeIndex &index) {
    std::error_code error;
    if (!fs::is_directory(root, error)) {
        return;
    }
    for (const auto &entry : fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, error)) {
        if (error || !entry.is_regular_file(error)) {
            continue;
        }
        const fs::path &path = entry.path();
        if (path.extension() == ".png") {
            index.png.emplace(path.stem().string(), path.string());
        } else if (path.extension() == ".svg") {
            index.svg.emplace(path.stem().string(), path.string());
        }
    }
}

const ThemeIndex &theme_index(const std::string &theme) {
    static std::unordered_map<std::string, ThemeIndex> indices;
    if (auto it = indices.find(theme); it != indices.end()) {
        return it->second;
    }
    ThemeIndex index;
    std::string home = home_dir();
    if (!home.empty()) {
        index_root(home + "/.local/share/icons/" + theme, index);
    }
    index_root("/usr/share/icons/" + theme, index);
    return indices.emplace(theme, std::move(index)).first->second;
}

const std::vector<std::string> &theme_order() {
    static const std::vector<std::string> order = icon_theme_order(active_gtk_theme());
    return order;
}

} // namespace

std::vector<std::string> icon_theme_order(const std::string &active) {
    std::vector<std::string> themes;
    if (!active.empty() && active != "hicolor") {
        themes.push_back(active);
    }
    for (const char *theme : {"Adwaita", "AdwaitaLegacy", "breeze", "breeze-dark"}) {
        if (std::ranges::find(themes, theme) == themes.end()) {
            themes.emplace_back(theme);
        }
    }
    themes.emplace_back("hicolor");
    return themes;
}

std::string resolve_app_icon_path(const std::string &icon_field) {
    if (icon_field.empty()) {
        return "";
    }
    std::error_code error;
    if (icon_field.starts_with('/')) {
        return fs::exists(icon_field, error) ? icon_field : "";
    }
    for (const std::string &theme : theme_order()) {
        const ThemeIndex &index = theme_index(theme);
        if (auto it = index.png.find(icon_field); it != index.png.end()) {
            return it->second;
        }
    }
    for (const std::string &theme : theme_order()) {
        const ThemeIndex &index = theme_index(theme);
        if (auto it = index.svg.find(icon_field); it != index.svg.end()) {
            return it->second;
        }
    }
    for (const char *extension : {".png", ".svg"}) {
        std::string pixmap = "/usr/share/pixmaps/" + icon_field + extension;
        if (fs::exists(pixmap, error)) {
            return pixmap;
        }
    }
    return "";
}

IconSurface load_app_icon(const std::string &path, int size) {
    if (path.empty()) {
        return nullptr;
    }
    auto surface = decode_image(path, size);
    return surface ? std::move(*surface) : nullptr;
}

} // namespace astralia
