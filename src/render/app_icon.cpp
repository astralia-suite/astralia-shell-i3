#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>

#include "render/app_icon.h"

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

std::string to_lower(std::string text) {
    for (char &c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::vector<std::string> desktop_app_dirs() {
    std::vector<std::string> dirs;
    const char *data_home = getenv("XDG_DATA_HOME");
    std::string home = home_dir();
    if (data_home != nullptr && *data_home != '\0') {
        dirs.push_back(std::string(data_home) + "/applications");
    } else if (!home.empty()) {
        dirs.push_back(home + "/.local/share/applications");
    }
    const char *data_dirs = getenv("XDG_DATA_DIRS");
    std::string list = data_dirs != nullptr && *data_dirs != '\0' ? data_dirs : "/usr/local/share:/usr/share";
    std::size_t start = 0;
    while (start <= list.size()) {
        std::size_t colon = list.find(':', start);
        std::string dir = list.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
        if (!dir.empty()) {
            dirs.push_back(dir + "/applications");
        }
        if (colon == std::string::npos) {
            break;
        }
        start = colon + 1;
    }
    return dirs;
}

void index_desktop_file(const fs::path &path, std::unordered_map<std::string, std::string> &index) {
    std::ifstream file(path);
    std::string line;
    std::string icon;
    std::string startup_class;
    bool in_section = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            if (in_section) {
                break;
            }
            in_section = line == "[Desktop Entry]";
            continue;
        }
        std::size_t equals = line.find('=');
        if (!in_section || equals == std::string::npos) {
            continue;
        }
        std::string_view key = std::string_view(line).substr(0, equals);
        if (key == "Icon") {
            icon = line.substr(equals + 1);
        } else if (key == "StartupWMClass") {
            startup_class = line.substr(equals + 1);
        }
    }
    if (icon.empty()) {
        return;
    }
    index.emplace(to_lower(path.stem().string()), icon);
    if (!startup_class.empty()) {
        index.emplace(to_lower(startup_class), icon);
    }
}

const std::unordered_map<std::string, std::string> &window_class_index() {
    static const std::unordered_map<std::string, std::string> index = [] {
        std::unordered_map<std::string, std::string> result;
        std::error_code error;
        for (const std::string &dir : desktop_app_dirs()) {
            if (!fs::is_directory(dir, error)) {
                continue;
            }
            for (const auto &entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, error)) {
                if (!error && entry.is_regular_file(error) && entry.path().extension() == ".desktop") {
                    index_desktop_file(entry.path(), result);
                }
            }
        }
        return result;
    }();
    return index;
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

std::string resolve_window_icon_path(const std::string &window_class) {
    if (!window_class.empty()) {
        const auto &index = window_class_index();
        if (auto it = index.find(to_lower(window_class)); it != index.end()) {
            if (std::string path = resolve_app_icon_path(it->second); !path.empty()) {
                return path;
            }
        }
        if (std::string path = resolve_app_icon_path(window_class); !path.empty()) {
            return path;
        }
    }
    return resolve_app_icon_path("application-x-executable");
}

IconSurface load_app_icon(const std::string &path, int size) {
    if (path.empty()) {
        return nullptr;
    }
    auto surface = decode_image(path, size);
    return surface ? std::move(*surface) : nullptr;
}

} // namespace astralia
