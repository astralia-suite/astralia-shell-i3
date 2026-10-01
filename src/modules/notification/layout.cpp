#include <cmath>
#include <numeric>

#include "config/notification_config.h"

#include "modules/notification/layout.h"

namespace astralia {

namespace cfg = notification_config;

double notification_card_height(double app_height, double summary_height, double body_height) {
    double height = cfg::card_pad * 2.0 + cfg::extra_height + app_height;
    if (summary_height > 0.0) {
        height += cfg::content_spacing + summary_height;
    }
    if (body_height > 0.0) {
        height += cfg::content_spacing + body_height;
    }
    return height;
}

std::size_t notification_fit_count(std::span<const double> heights) {
    std::size_t count = 0;
    double stack = 0.0;
    for (auto it = heights.rbegin(); it != heights.rend(); ++it) {
        stack += *it + (count > 0 ? cfg::spacing : 0.0);
        if (count > 0 && stack > cfg::max_stack_height) {
            break;
        }
        ++count;
    }
    return count;
}

double notification_stack_height(std::span<const double> heights) {
    if (heights.empty()) {
        return 0.0;
    }
    return std::accumulate(heights.begin(), heights.end(), 0.0) +
           cfg::spacing * static_cast<double>(heights.size() - 1);
}

StackOrigin notification_stack_origin(const OutputGeometry &output, double stack_height) {
    double scale = ui_scale(output);
    return {output.x + output.width - static_cast<int>(std::lround(cfg::margin_right * scale)) -
                static_cast<int>(std::ceil(cfg::card_width * scale)),
            output.y + output.height - static_cast<int>(std::lround(cfg::margin_bottom * scale)) -
                static_cast<int>(std::ceil(stack_height * scale))};
}

std::optional<std::size_t> notification_close_at(double x, double y, std::span<const double> heights) {
    constexpr double hit = cfg::card_pad * 2.0 + cfg::close_size;
    if (x < cfg::card_width - hit || x >= cfg::card_width) {
        return std::nullopt;
    }
    double top = 0.0;
    for (std::size_t i = 0; i < heights.size(); ++i) {
        if (y >= top && y < top + hit) {
            return i;
        }
        top += heights[i] + cfg::spacing;
    }
    return std::nullopt;
}

} // namespace astralia
