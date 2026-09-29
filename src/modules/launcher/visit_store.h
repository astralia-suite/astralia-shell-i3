#pragma once

#include <string>

#include "config/launcher_config.h"

namespace astralia {

std::string visit_store_app_key(const std::string &desktop_id);
std::string visit_store_file_key(const std::string &file_path);
std::string visit_store_path(const char *state_home, const char *home);
VisitStore visit_store_load(const std::string &path);
int visit_store_get(const VisitStore &store, const std::string &key);
void visit_store_record(VisitStore &store, const std::string &key);

} // namespace astralia
