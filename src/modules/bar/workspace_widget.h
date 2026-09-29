#pragma once

#include <cairo.h>
#include <cstdint>
#include <optional>

#include "service/workspace_service.h"

namespace astralia {

int workspace_row_width(const WorkspaceStatus &status);
std::optional<uint32_t> workspace_at(const WorkspaceStatus &status, int offset);

void draw_workspace_row(cairo_t *cr, const WorkspaceStatus &status, double x, int top, int height);

} // namespace astralia
