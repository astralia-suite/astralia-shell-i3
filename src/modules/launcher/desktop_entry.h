#pragma once

#include <istream>
#include <optional>
#include <string>
#include <vector>

#include "config/launcher_config.h"

namespace astralia {

std::optional<DesktopEntry> parse_desktop_entry(std::istream &in, const std::string &id);
std::string strip_exec_field_codes(const std::string &exec);
std::vector<std::string> desktop_entry_dirs(const char *data_home, const char *data_dirs,
                                            const char *home);
std::vector<DesktopEntry> scan_desktop_entries();

} // namespace astralia
