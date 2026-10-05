#include <array>
#include <utility>

#include "core/log.h"

#include "service/settings_service.h"

namespace astralia {

namespace {

constexpr std::array<std::string_view, feature_count> feature_names{"bar", "osd", "notifications"};

std::optional<Feature> feature_from(std::string_view name) {
    for (std::size_t i = 0; i < feature_count; ++i) {
        if (feature_names[i] == name) {
            return static_cast<Feature>(i);
        }
    }
    return std::nullopt;
}

std::optional<bool> flag_from(std::string_view value) {
    if (value == "on") {
        return true;
    }
    if (value == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<BarStyle> bar_style_from(std::string_view value) {
    for (std::size_t i = 0; i < bar_config::style_count; ++i) {
        if (bar_config::style_names[i] == value) {
            return static_cast<BarStyle>(i);
        }
    }
    return std::nullopt;
}

std::string_view flag_text(bool value) { return value ? "on" : "off"; }

std::string initial_text(const std::string &path) {
    if (std::optional<std::string> text = read_text_file(path)) {
        return std::move(*text);
    }
    log::info("writing default settings to {}", path);
    write_text_file(path, settings_config::default_text);
    return settings_config::default_text;
}

} // namespace

std::string_view feature_name(Feature feature) {
    return feature_names[static_cast<std::size_t>(feature)];
}

std::string feature_key(Feature feature, const std::string &output) {
    std::string name(feature_name(feature));
    return output.empty() ? name : output + "." + name;
}

SettingsFile parse_settings_file(std::string_view text) {
    SettingsFile file;
    ConfigLines lines = parse_config_lines(text);
    file.invalid_lines = std::move(lines.invalid);
    for (const ConfigEntry &entry : lines.entries) {
        if (entry.key == settings_config::wallpaper_dir_key) {
            file.wallpaper_dir = entry.value;
            continue;
        }
        if (entry.key == settings_config::bar_style_key) {
            if (std::optional<BarStyle> style = bar_style_from(entry.value)) {
                file.bar_style = *style;
            } else {
                file.invalid_lines.push_back(entry.line);
            }
            continue;
        }
        std::size_t dot = entry.key.rfind('.');
        std::string output = dot == std::string::npos ? "" : entry.key.substr(0, dot);
        std::string_view name = dot == std::string::npos
                                    ? std::string_view(entry.key)
                                    : std::string_view(entry.key).substr(dot + 1);
        std::optional<Feature> feature = feature_from(name);
        std::optional<bool> value = flag_from(entry.value);
        if (!feature || !value || (dot != std::string::npos && output.empty())) {
            file.invalid_lines.push_back(entry.line);
            continue;
        }
        auto index = static_cast<std::size_t>(*feature);
        if (output.empty()) {
            file.defaults[index] = *value;
        } else {
            file.overrides[output][index] = *value;
        }
    }
    return file;
}

bool feature_enabled(const SettingsFile &file, Feature feature, const std::string &output) {
    auto index = static_cast<std::size_t>(feature);
    if (auto it = file.overrides.find(output); it != file.overrides.end() && it->second[index]) {
        return *it->second[index];
    }
    return file.defaults[index];
}

SettingsService::SettingsService(EventLoop &loop)
    : path_(user_config_path(settings_config::file)), text_(initial_text(path_)),
      file_(parse_settings_file(text_)), watch_(loop, path_, [this] { reload(); }) {
    for (std::size_t line : file_.invalid_lines) {
        log::error("{}:{}: expected `key = on|off`, `bar_style = continuous|okinami`", path_, line);
    }
}

bool SettingsService::enabled(Feature feature, const std::string &output) const {
    return feature_enabled(file_, feature, output);
}

bool SettingsService::overridden(const std::string &output) const {
    return file_.overrides.contains(output);
}

void SettingsService::set(Feature feature, const std::string &output, bool value) {
    store(with_entry(text_, feature_key(feature, output), flag_text(value)));
}

void SettingsService::set_override(const std::string &output, bool on) {
    if (output.empty()) {
        return;
    }
    std::string text = text_;
    for (std::size_t i = 0; i < feature_count; ++i) {
        auto feature = static_cast<Feature>(i);
        if (on) {
            text = with_entry(text, feature_key(feature, output),
                              flag_text(feature_enabled(file_, feature, output)));
        } else {
            text = without_entry(text, feature_key(feature, output));
        }
    }
    store(std::move(text));
}

void SettingsService::set_wallpaper_dir(const std::string &dir) {
    if (dir.empty()) {
        return;
    }
    store(with_entry(text_, settings_config::wallpaper_dir_key, dir));
}

void SettingsService::set_bar_style(BarStyle style) {
    store(with_entry(text_, settings_config::bar_style_key, bar_config::style_names[static_cast<std::size_t>(style)]));
}

void SettingsService::store(std::string text) {
    if (!write_text_file(path_, text)) {
        return;
    }
    text_ = std::move(text);
    SettingsFile next = parse_settings_file(text_);
    if (next == file_) {
        return;
    }
    file_ = std::move(next);
    changed.emit();
}

void SettingsService::reload() {
    std::optional<std::string> text = read_text_file(path_);
    if (!text) {
        log::info("settings file removed, writing defaults to {}", path_);
        store(settings_config::default_text);
        return;
    }
    if (*text == text_) {
        return;
    }
    log::info("reloading {}", path_);
    text_ = std::move(*text);
    SettingsFile next = parse_settings_file(text_);
    if (next == file_) {
        return;
    }
    file_ = std::move(next);
    changed.emit();
}

} // namespace astralia
