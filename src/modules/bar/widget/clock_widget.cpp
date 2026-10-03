#include <cstddef>
#include <ctime>
#include <string_view>

#include "config/bar_config.h"

#include "modules/bar/widget/clock_widget.h"

#include "render/icons.h"

namespace astralia {

std::chrono::milliseconds ms_until_next_second(std::chrono::system_clock::time_point now) {
    auto next = std::chrono::floor<std::chrono::seconds>(now) + std::chrono::seconds(1);
    return std::chrono::ceil<std::chrono::milliseconds>(next - now);
}

ClockWidget::ClockWidget() { set_icon(icon::clock); }

bool ClockWidget::refresh() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_r(&now, &local);
    char buffer[64]{};
    std::size_t length = std::strftime(buffer, sizeof buffer, bar_config::clock_format, &local);
    return set_label(std::string_view(buffer, length));
}

} // namespace astralia
