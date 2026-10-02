#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace astralia::log {

void write(std::string_view level, std::string_view message);

template <typename... Args>
void info(std::format_string<Args...> fmt, Args &&...args) {
    write("info", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args &&...args) {
    write("error", std::format(fmt, std::forward<Args>(args)...));
}

} // namespace astralia::log
