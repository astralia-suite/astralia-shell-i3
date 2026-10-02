#pragma once

#include <cairo.h>
#include <cstdint>
#include <optional>

#include "service/i3_service.h"

namespace astralia {

int workspace_row_width(const I3Status &status);
std::optional<uint32_t> workspace_at(const I3Status &status, int offset);

void draw_workspace_row(cairo_t *cr, const I3Status &status, double x, int top, int height);

} // namespace astralia
