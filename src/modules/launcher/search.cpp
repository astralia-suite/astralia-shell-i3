#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include "modules/launcher/apps_provider.h"
#include "modules/launcher/search.h"
#include "modules/launcher/visit_store.h"

namespace astralia {

namespace {

struct Prefix {
    std::string_view text;
    LauncherMode mode;
};

constexpr std::array<Prefix, 5> prefixes{{
    {">", LauncherMode::run},
    {"gg", LauncherMode::google},
    {"ddg", LauncherMode::duckduckgo},
    {"yt", LauncherMode::youtube},
    {"url", LauncherMode::url},
}};

bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string trim_left(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && is_space(s[i])) {
        ++i;
    }
    return std::string(s.substr(i));
}

bool is_word(std::string_view s) {
    return std::ranges::all_of(s, [](unsigned char c) { return std::isalnum(c) != 0; });
}

} // namespace

ModeQuery detect_mode_and_query(const std::string &raw) {
    std::string trimmed = trim_left(raw);
    if (trimmed.empty()) {
        return {LauncherMode::drun, ""};
    }
    for (const Prefix &prefix : prefixes) {
        if (!trimmed.starts_with(prefix.text)) {
            continue;
        }
        if (is_word(prefix.text) && trimmed.size() > prefix.text.size() &&
            !is_space(trimmed[prefix.text.size()])) {
            continue;
        }
        return {prefix.mode, trim_left(std::string_view(trimmed).substr(prefix.text.size()))};
    }
    return {LauncherMode::drun, trimmed};
}

std::vector<DrunResult> combined_drun_results(const std::vector<ScoredApp> &apps,
                                              const std::vector<FileEntry> &files,
                                              const VisitStore &visits, int max_results) {
    struct Ranked {
        DrunResult result;
        int tier;
        int visits;
        float score;
        std::string name;
    };

    std::vector<Ranked> ranked;
    ranked.reserve(apps.size() + files.size());
    for (const ScoredApp &app : apps) {
        ranked.push_back({{DrunResult::Kind::app, app.entry, {}},
                          0,
                          visit_store_get(visits, visit_store_app_key(app.entry->id)),
                          app.score,
                          to_lower(app.entry->name)});
    }
    for (const FileEntry &file : files) {
        ranked.push_back(
            {{file.is_dir ? DrunResult::Kind::dir : DrunResult::Kind::file, nullptr, file},
             file.is_dir ? 1 : 2,
             visit_store_get(visits, visit_store_file_key(file.path)),
             file.score,
             to_lower(file.name)});
    }
    std::ranges::stable_sort(ranked, [](const Ranked &a, const Ranked &b) {
        if (a.tier != b.tier) {
            return a.tier < b.tier;
        }
        if (a.visits != b.visits) {
            return a.visits > b.visits;
        }
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.name < b.name;
    });

    std::vector<DrunResult> out;
    for (std::size_t i = 0; i < ranked.size() && static_cast<int>(i) < max_results; ++i) {
        out.push_back(std::move(ranked[i].result));
    }
    return out;
}

} // namespace astralia
