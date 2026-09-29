#include <cstdlib>

#include "core/runtime_paths.h"

namespace astralia {

std::string runtime_path(std::string_view display, std::string_view runtime_dir,
                         std::string_view suffix) {
    std::string path(runtime_dir.empty() ? "/tmp" : runtime_dir);
    path += "/astralia-shell";
    std::string id;
    for (char c : display) {
        if (c == ':') {
            continue;
        }
        id += c == '/' ? '_' : c;
    }
    if (!id.empty()) {
        path += '-';
        path += id;
    }
    path += suffix;
    return path;
}

std::string runtime_path(std::string_view suffix) {
    const char *display = std::getenv("DISPLAY");
    const char *runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    return runtime_path(display ? display : "", runtime_dir ? runtime_dir : "", suffix);
}

} // namespace astralia
