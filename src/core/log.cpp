#include <cstdio>
#include <print>

#include "core/log.h"

namespace astralia::log {

void write(std::string_view level, std::string_view message) {
    std::println(stderr, "astralia-shell [{}] {}", level, message);
}

} // namespace astralia::log
