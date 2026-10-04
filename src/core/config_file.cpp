#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <sys/inotify.h>
#include <system_error>
#include <unistd.h>

#include "core/config_file.h"
#include "core/log.h"

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

std::string_view entry_key(std::string_view line) {
    line = trim(line);
    std::size_t equals = line.find('=');
    if (line.empty() || line.front() == '#' || equals == std::string_view::npos) {
        return {};
    }
    return trim(line.substr(0, equals));
}

template <typename Visit>
void for_each_line(std::string_view text, Visit visit) {
    while (!text.empty()) {
        std::size_t end = text.find('\n');
        visit(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    }
}

} // namespace

std::string config_file_path(std::string_view config_home, std::string_view home,
                             std::string_view relative) {
    if (!config_home.empty()) {
        return std::format("{}/{}", config_home, relative);
    }
    return std::format("{}/.config/{}", home, relative);
}

std::string user_config_path(std::string_view relative) {
    const char *config_home = std::getenv("XDG_CONFIG_HOME");
    const char *home = std::getenv("HOME");
    return config_file_path(config_home != nullptr ? config_home : "", home != nullptr ? home : "",
                            relative);
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

ConfigLines parse_config_lines(std::string_view text) {
    ConfigLines result;
    std::size_t number = 0;
    for_each_line(text, [&](std::string_view raw) {
        ++number;
        std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') {
            return;
        }
        std::size_t equals = line.find('=');
        std::string_view key = trim(line.substr(0, equals));
        std::string_view value =
            equals == std::string_view::npos ? std::string_view{} : trim(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            result.invalid.push_back(number);
            return;
        }
        result.entries.push_back({number, std::string(key), std::string(value)});
    });
    return result;
}

std::string with_entry(std::string_view text, std::string_view key, std::string_view value) {
    std::string result;
    bool replaced = false;
    for_each_line(text, [&](std::string_view line) {
        if (!replaced && entry_key(line) == key) {
            result += std::format("{} = {}\n", key, value);
            replaced = true;
            return;
        }
        result += line;
        result += '\n';
    });
    if (!replaced) {
        result += std::format("{} = {}\n", key, value);
    }
    return result;
}

std::string without_entry(std::string_view text, std::string_view key) {
    std::string result;
    for_each_line(text, [&](std::string_view line) {
        if (entry_key(line) == key) {
            return;
        }
        result += line;
        result += '\n';
    });
    return result;
}

std::optional<std::string> read_text_file(const std::string &path) {
    std::ifstream stream(path);
    if (!stream) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(stream), {});
}

bool write_text_file(const std::string &path, std::string_view text) {
    std::filesystem::path target(path);
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    std::filesystem::path temporary = target;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::trunc);
        stream << text;
        if (!stream) {
            log::error("cannot write {}", temporary.string());
            return false;
        }
    }
    std::filesystem::rename(temporary, target, error);
    if (error) {
        log::error("cannot replace {}: {}", path, error.message());
        return false;
    }
    return true;
}

FileWatch::FileWatch(EventLoop &loop, const std::string &path, std::function<void()> changed)
    : loop_(loop) {
    std::filesystem::path file(path);
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    inotify_ = UniqueFd(inotify_init1(IN_NONBLOCK | IN_CLOEXEC));
    if (inotify_.get() < 0 ||
        inotify_add_watch(inotify_.get(), file.parent_path().c_str(),
                          IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE) < 0) {
        log::error("cannot watch {}: {}", file.parent_path().string(), std::strerror(errno));
        inotify_ = UniqueFd();
        return;
    }
    name_ = file.filename().string();
    loop_.on_fd(inotify_.get(), [this, changed = std::move(changed)] {
        if (touched()) {
            changed();
        }
    });
}

FileWatch::~FileWatch() {
    if (inotify_.get() >= 0) {
        loop_.remove_fd(inotify_.get());
    }
}

bool FileWatch::touched() {
    alignas(inotify_event) std::array<char, 4096> buffer;
    bool changed = false;
    ssize_t length = 0;
    while ((length = read(inotify_.get(), buffer.data(), buffer.size())) > 0) {
        for (ssize_t offset = 0; offset < length;) {
            const auto *event = reinterpret_cast<const inotify_event *>(buffer.data() + offset);
            if (event->len > 0 && name_ == event->name) {
                changed = true;
            }
            offset += sizeof(inotify_event) + event->len;
        }
    }
    return changed;
}

} // namespace astralia
