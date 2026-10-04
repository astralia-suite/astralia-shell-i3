#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "config/settings_config.h"

#include "core/config_file.h"
#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

enum class Feature : std::size_t { bar,
                                   osd,
                                   notifications };

inline constexpr std::size_t feature_count = 3;

struct SettingsFile {
    std::array<bool, feature_count> defaults{true, true, true};
    std::unordered_map<std::string, std::array<std::optional<bool>, feature_count>> overrides;
    std::string wallpaper_dir = settings_config::default_wallpaper_dir;
    std::vector<std::size_t> invalid_lines;

    bool operator==(const SettingsFile &) const = default;
};

std::string_view feature_name(Feature feature);
std::string feature_key(Feature feature, const std::string &output);
SettingsFile parse_settings_file(std::string_view text);
bool feature_enabled(const SettingsFile &file, Feature feature, const std::string &output);

class SettingsService {
  public:
    explicit SettingsService(EventLoop &loop);

    bool enabled(Feature feature, const std::string &output) const;
    bool overridden(const std::string &output) const;
    void set(Feature feature, const std::string &output, bool value);
    void set_override(const std::string &output, bool on);
    const std::string &wallpaper_dir() const { return file_.wallpaper_dir; }
    void set_wallpaper_dir(const std::string &dir);

    Signal<> changed;

  private:
    void store(std::string text);
    void reload();

    std::string path_;
    std::string text_;
    SettingsFile file_;
    FileWatch watch_;
};

} // namespace astralia
