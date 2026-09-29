#pragma once

#include <string>
#include <string_view>

namespace astralia {

std::string runtime_path(std::string_view display, std::string_view runtime_dir,
                         std::string_view suffix);
std::string runtime_path(std::string_view suffix);

} // namespace astralia
