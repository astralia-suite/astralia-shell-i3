#include <algorithm>
#include <array>
#include <cstdio>
#include <sstream>

#include "config/launcher_config.h"

#include "modules/launcher/apps_provider.h"
#include "modules/launcher/files_provider.h"
#include "modules/launcher/launch_action.h"

namespace astralia {

namespace {

std::string trim_spaces(const std::string &s) {
    std::size_t b = s.find_first_not_of(' ');
    std::size_t e = s.find_last_not_of(' ');
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

std::vector<std::size_t> char_starts(const std::string &s) {
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            starts.push_back(i);
        }
    }
    return starts;
}

std::string run_command(const std::vector<std::string> &argv) {
    std::string cmd;
    for (const std::string &arg : argv) {
        cmd += shell_quote(arg) + ' ';
    }
    cmd += "2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return {};
    }
    std::string out;
    std::array<char, 4096> buf{};
    std::size_t n = 0;
    while ((n = fread(buf.data(), 1, buf.size(), pipe)) > 0) {
        out.append(buf.data(), n);
    }
    pclose(pipe);
    return out;
}

} // namespace

std::string basename_of(const std::string &path) {
    std::string p = path;
    while (p.size() > 1 && p.back() == '/') {
        p.pop_back();
    }
    if (p == "/") {
        return p;
    }
    std::size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

std::string parent_of(const std::string &path) {
    std::string p = path;
    while (p.size() > 1 && p.back() == '/') {
        p.pop_back();
    }
    std::size_t slash = p.find_last_of('/');
    return slash == std::string::npos || slash == 0 ? "/" : p.substr(0, slash);
}

std::string collapse_home(const std::string &path, const std::string &home) {
    if (home.empty() || home == "/" || !path.starts_with(home)) {
        return path;
    }
    if (path.size() == home.size()) {
        return "~";
    }
    return path[home.size()] == '/' ? "~" + path.substr(home.size()) : path;
}

std::string elide(const std::string &s, std::size_t max_chars) {
    std::vector<std::size_t> starts = char_starts(s);
    if (starts.size() <= max_chars || max_chars == 0) {
        return s;
    }
    return s.substr(0, starts[max_chars - 1]) + "…";
}

std::string elide_middle(const std::string &s, std::size_t max_chars) {
    std::vector<std::size_t> starts = char_starts(s);
    if (starts.size() <= max_chars) {
        return s;
    }
    if (max_chars < 2) {
        return elide(s, max_chars);
    }
    std::size_t keep = max_chars - 1;
    std::size_t head = (keep + 1) / 2;
    std::size_t tail = keep - head;
    return s.substr(0, starts[head]) + "…" + s.substr(starts[starts.size() - tail]);
}

std::string to_glob_pattern(const std::string &query) {
    std::string q = trim_spaces(query);
    if (q.empty()) {
        return "";
    }
    if (q.starts_with("**/") || q.starts_with("/")) {
        return q;
    }
    if (q.find('/') != std::string::npos) {
        return "**/" + q;
    }
    return "**/*" + q + "*";
}

std::vector<std::string> split_query_parts(const std::string &query) {
    std::string q = trim_spaces(to_lower(query));
    if (q.empty()) {
        return {};
    }
    if (q.find('*') == std::string::npos) {
        return {q};
    }
    std::vector<std::string> parts;
    std::stringstream ss(q);
    std::string part;
    while (std::getline(ss, part, '*')) {
        part = trim_spaces(part);
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

float score_path(const std::string &name, const std::string &query) {
    std::string n = to_lower(name);
    std::vector<std::string> parts = split_query_parts(query);
    if (n.empty() || parts.empty()) {
        return -1.0f;
    }
    float score = 0.0f;
    std::size_t cursor = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        std::size_t idx = n.find(parts[i], cursor);
        if (idx == std::string::npos) {
            return -1.0f;
        }
        if (i == 0) {
            score += idx == 0 ? 1000.0f : 500.0f;
            score -= static_cast<float>(std::min(idx, std::size_t{200}));
        } else {
            std::size_t end_distance = n.size() - (idx + parts[i].size());
            score += 200.0f;
            score -= static_cast<float>(std::min(end_distance, std::size_t{200}));
        }
        cursor = idx + parts[i].size();
    }
    score -= static_cast<float>(std::min(n.size(), std::size_t{200})) / 10.0f;
    return score;
}

std::vector<std::string> fd_search_argv(const std::string &pattern, const std::string &root,
                                        bool is_dir, int max_results, int depth, bool full_path) {
    std::vector<std::string> argv = {"fd", "--glob", "--ignore-case"};
    if (full_path) {
        argv.push_back("--full-path");
    }
    argv.insert(argv.end(),
                {"--type", is_dir ? "d" : "f", "--hidden", "--no-ignore", "--absolute-path",
                 "--color", "never", "--max-results", std::to_string(max_results)});
    if (depth > 0) {
        argv.insert(argv.end(), {"--max-depth", std::to_string(depth)});
    }
    argv.insert(argv.end(), {"--", pattern.empty() ? "*" : pattern, root});
    return argv;
}

std::vector<FileEntry> fd_search_parse_output(const std::string &raw, bool is_dir) {
    std::vector<FileEntry> results;
    std::stringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        std::size_t b = line.find_first_not_of(" \t");
        std::size_t e = line.find_last_not_of(" \t");
        if (b == std::string::npos) {
            continue;
        }
        std::string path = line.substr(b, e - b + 1);
        results.push_back({basename_of(path), path, is_dir, 0.0f});
    }
    return results;
}

std::vector<FileEntry> list_directory(const std::string &path, bool want_dirs) {
    std::string raw = run_command(
        fd_search_argv("", path, want_dirs, launcher_config::listing_max_results, 1, false));
    return fd_search_parse_output(raw, want_dirs);
}

} // namespace astralia
