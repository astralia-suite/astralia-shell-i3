#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/event_loop.h"
#include "core/unique_fd.h"

namespace astralia {

struct ConfigEntry {
    std::size_t line;
    std::string key;
    std::string value;
};

struct ConfigLines {
    std::vector<ConfigEntry> entries;
    std::vector<std::size_t> invalid;
};

std::string config_file_path(std::string_view config_home, std::string_view home,
                             std::string_view relative);
std::string user_config_path(std::string_view relative);
std::string expand_home(std::string_view path, std::string_view home);
ConfigLines parse_config_lines(std::string_view text);
std::string with_entry(std::string_view text, std::string_view key, std::string_view value);
std::string without_entry(std::string_view text, std::string_view key);
std::optional<std::string> read_text_file(const std::string &path);
bool write_text_file(const std::string &path, std::string_view text);

class FileWatch {
  public:
    FileWatch(EventLoop &loop, const std::string &path, std::function<void()> changed);
    ~FileWatch();
    FileWatch(const FileWatch &) = delete;
    FileWatch &operator=(const FileWatch &) = delete;

  private:
    bool touched();

    EventLoop &loop_;
    std::string name_;
    UniqueFd inotify_;
};

} // namespace astralia
