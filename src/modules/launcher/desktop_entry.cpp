#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "modules/launcher/desktop_entry.h"

namespace astralia {

std::optional<DesktopEntry> parse_desktop_entry(std::istream &in, const std::string &id) {
    DesktopEntry entry;
    entry.id = id;
    std::string line;
    bool in_section = false;
    bool application = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            in_section = line == "[Desktop Entry]";
            continue;
        }
        if (!in_section) {
            continue;
        }
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "Name" && entry.name.empty()) {
            entry.name = value;
        } else if (key == "Exec") {
            entry.exec = value;
        } else if (key == "Icon") {
            entry.icon = value;
        } else if (key == "Terminal") {
            entry.terminal = value == "true";
        } else if (key == "NoDisplay") {
            entry.no_display = value == "true";
        } else if (key == "Hidden") {
            entry.hidden = value == "true";
        } else if (key == "Type") {
            application = value == "Application";
        }
    }
    if (!application || entry.name.empty() || entry.exec.empty()) {
        return std::nullopt;
    }
    return entry;
}

std::string strip_exec_field_codes(const std::string &exec) {
    std::string out;
    out.reserve(exec.size());
    for (std::size_t i = 0; i < exec.size(); ++i) {
        if (exec[i] != '%') {
            out += exec[i];
            continue;
        }
        if (i + 1 >= exec.size()) {
            break;
        }
        char code = exec[++i];
        if (code == '%') {
            out += '%';
        } else if (i + 1 < exec.size() && exec[i + 1] == ' ' &&
                   (i + 2 >= exec.size() || exec[i + 2] != '%')) {
            ++i;
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::vector<std::string> desktop_entry_dirs(const char *data_home, const char *data_dirs,
                                            const char *home) {
    std::vector<std::string> dirs;
    if (data_home != nullptr && *data_home != '\0') {
        dirs.push_back(std::string(data_home) + "/applications");
    } else if (home != nullptr && *home != '\0') {
        dirs.push_back(std::string(home) + "/.local/share/applications");
    }
    std::stringstream joined(
        data_dirs != nullptr && *data_dirs != '\0' ? data_dirs : "/usr/local/share:/usr/share");
    std::string part;
    while (std::getline(joined, part, ':')) {
        if (!part.empty()) {
            dirs.push_back(part + "/applications");
        }
    }
    return dirs;
}

std::vector<DesktopEntry> scan_desktop_entries() {
    std::vector<DesktopEntry> result;
    std::unordered_set<std::string> seen;
    for (const std::string &dir :
         desktop_entry_dirs(getenv("XDG_DATA_HOME"), getenv("XDG_DATA_DIRS"), getenv("HOME"))) {
        DIR *handle = opendir(dir.c_str());
        if (handle == nullptr) {
            continue;
        }
        while (dirent *ent = readdir(handle)) {
            std::string name = ent->d_name;
            if (!name.ends_with(".desktop") || !seen.insert(name).second) {
                continue;
            }
            std::ifstream file(dir + "/" + name);
            auto entry = parse_desktop_entry(file, name);
            if (entry && !entry->no_display && !entry->hidden) {
                result.push_back(std::move(*entry));
            }
        }
        closedir(handle);
    }
    return result;
}

} // namespace astralia
