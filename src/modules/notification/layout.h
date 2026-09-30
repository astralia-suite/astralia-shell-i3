#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "core/x_connection.h"

namespace astralia {

struct StackOrigin {
    int x;
    int y;
};

double notification_card_height(double app_height, double summary_height, double body_height);
std::size_t notification_fit_count(std::span<const double> heights);
double notification_stack_height(std::span<const double> heights);
StackOrigin notification_stack_origin(const OutputGeometry &output, double stack_height);
std::optional<std::size_t> notification_close_at(double x, double y, std::span<const double> heights);

} // namespace astralia
