#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "config/launcher_config.h"

namespace astralia {

std::string basename_of(const std::string &path);
std::string parent_of(const std::string &path);
std::string collapse_home(const std::string &path, const std::string &home);
std::string elide(const std::string &s, std::size_t max_chars);
std::string elide_middle(const std::string &s, std::size_t max_chars);
std::string to_glob_pattern(const std::string &query);
std::vector<std::string> split_query_parts(const std::string &query);
float score_path(const std::string &name, const std::string &query);
std::vector<std::string> fd_search_argv(const std::string &pattern, const std::string &root,
                                        bool is_dir, int max_results, int depth = -1,
                                        bool full_path = true);
std::vector<FileEntry> fd_search_parse_output(const std::string &raw, bool is_dir);
std::vector<FileEntry> list_directory(const std::string &path, bool want_dirs);

} // namespace astralia
