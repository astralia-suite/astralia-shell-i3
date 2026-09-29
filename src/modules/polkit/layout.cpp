#include <algorithm>

#include "config/polkit_config.h"

#include "modules/polkit/layout.h"

namespace astralia {

namespace cfg = polkit_config;

std::size_t utf8_length(std::string_view text) {
    return static_cast<std::size_t>(std::ranges::count_if(
        text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

double polkit_card_height(bool show_info) {
    double height = cfg::card_pad * 2.0 + cfg::title_line_height + cfg::spacing +
                    cfg::message_line_height + cfg::spacing + cfg::field_height;
    return show_info ? height + cfg::spacing + cfg::info_line_height : height;
}

std::size_t polkit_visible_dots(std::size_t length, double width) {
    return std::min(length, static_cast<std::size_t>(std::max(width, 0.0) / cfg::dot_size));
}

} // namespace astralia
