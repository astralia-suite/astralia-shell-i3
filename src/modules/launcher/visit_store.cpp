#include <filesystem>
#include <fstream>

#include "core/log.h"

#include "modules/launcher/visit_store.h"

namespace astralia {

std::string visit_store_app_key(const std::string &desktop_id) { return "app:" + desktop_id; }

std::string visit_store_file_key(const std::string &file_path) { return "file:" + file_path; }

std::string visit_store_path(const char *state_home, const char *home) {
    std::string dir = state_home != nullptr && *state_home != '\0'
                          ? std::string(state_home)
                          : std::string(home != nullptr ? home : "") + "/.local/state";
    return dir + "/" + launcher_config::visits_file;
}

VisitStore visit_store_load(const std::string &path) {
    VisitStore store;
    store.path = path;
    std::ifstream file(path);
    std::string key;
    int count = 0;
    while (file >> key >> count) {
        store.counts[key] = count;
    }
    return store;
}

int visit_store_get(const VisitStore &store, const std::string &key) {
    auto it = store.counts.find(key);
    return it == store.counts.end() ? 0 : it->second;
}

void visit_store_record(VisitStore &store, const std::string &key) {
    if (key.empty()) {
        return;
    }
    ++store.counts[key];
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(store.path).parent_path(), error);
    std::ofstream file(store.path, std::ios::trunc);
    if (!file) {
        log::error("launcher: cannot write {}", store.path);
        return;
    }
    for (const auto &[k, count] : store.counts) {
        file << k << '\t' << count << '\n';
    }
}

} // namespace astralia
