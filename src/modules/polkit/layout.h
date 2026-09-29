#pragma once

#include <cstddef>
#include <string_view>

namespace astralia {

std::size_t utf8_length(std::string_view text);
double polkit_card_height(bool show_info);
std::size_t polkit_visible_dots(std::size_t length, double width);

} // namespace astralia
