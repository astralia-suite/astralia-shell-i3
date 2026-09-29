#pragma once

#include <span>
#include <string>
#include <string_view>

namespace astralia {

enum class Mode { daemon, debug, client };

struct Invocation {
    Mode mode;
    std::string command;
};

Invocation parse_invocation(std::span<const std::string_view> args);

} // namespace astralia
