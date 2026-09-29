#pragma once

#include <optional>

namespace astralia {

struct Point {
    double x;
    double y;
};

Point logout_button_center(int index, Point center);
std::optional<int> logout_button_at(Point pointer, Point center);

} // namespace astralia
