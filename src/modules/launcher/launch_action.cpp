#include <cctype>
#include <cstdlib>
#include <regex>

#include "core/spawn.h"

#include "modules/launcher/desktop_entry.h"
#include "modules/launcher/launch_action.h"
#include "modules/launcher/visit_store.h"

namespace astralia {

namespace {

std::string trim(const std::string &s) {
    std::size_t b = s.find_first_not_of(" \t\n\r");
    if (b == std::string::npos) {
        return "";
    }
    return s.substr(b, s.find_last_not_of(" \t\n\r") - b + 1);
}

std::string url_encode(const std::string &text) {
    constexpr const char *hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if (std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

void spawn_with(const char *app, const std::string &arg) {
    spawn_detached(std::string(app) + " " + shell_quote(arg));
}

bool open_url(const std::string &url) {
    if (url.empty()) {
        return false;
    }
    spawn_with(launcher_config::browser, url);
    return true;
}

} // namespace

std::string shell_quote(const std::string &s) {
    std::string quoted = "'";
    for (char c : s) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    return quoted + "'";
}

std::string make_search_url(const std::string &text, const std::string &base) {
    std::string t = trim(text);
    return t.empty() ? "" : base + url_encode(t);
}

std::string normalize_url(const std::string &text) {
    std::string t = trim(text);
    if (t.empty()) {
        return "";
    }
    static const std::regex scheme(R"(^[a-zA-Z][a-zA-Z0-9+.-]*://)");
    if (std::regex_search(t, scheme)) {
        return t;
    }
    if (t.starts_with("//")) {
        return "https:" + t;
    }
    if (t.find_first_of(" \t") != std::string::npos) {
        return "";
    }
    static const std::regex localhost(R"(^localhost([:/].*)?$)");
    static const std::regex ipv4(R"(^\d{1,3}(?:\.\d{1,3}){3}([:/].*)?$)");
    static const std::regex host_tld(R"(^[^\s@]+\.[^\s@]+$)");
    static const std::regex host_port(R"(^[^\s/]+:\d+(?:/.*)?$)");
    for (const std::regex *pattern : {&localhost, &ipv4, &host_tld, &host_port}) {
        if (std::regex_match(t, *pattern)) {
            return "http://" + t;
        }
    }
    return "";
}

std::string app_command(const DesktopEntry &entry) {
    std::string command = strip_exec_field_codes(entry.exec);
    return entry.terminal ? std::string(launcher_config::terminal) + " " + command : command;
}

bool launch_non_drun(LauncherMode mode, const std::string &query) {
    switch (mode) {
    case LauncherMode::run: {
        std::string command = trim(query);
        if (command.empty()) {
            return false;
        }
        const char *shell = getenv("SHELL");
        spawn_detached(std::string(shell != nullptr ? shell : "/bin/sh") + " -lic " +
                       shell_quote(command));
        return true;
    }
    case LauncherMode::google:
        return open_url(make_search_url(query, launcher_config::google_url));
    case LauncherMode::duckduckgo:
        return open_url(make_search_url(query, launcher_config::duckduckgo_url));
    case LauncherMode::youtube:
        return open_url(make_search_url(query, launcher_config::youtube_url));
    case LauncherMode::url:
        return open_url(normalize_url(query));
    case LauncherMode::drun:
        return false;
    }
    return false;
}

void launch_submenu_action(const SubmenuEntry &entry, VisitStore &visits) {
    using Action = SubmenuEntry::Action;
    switch (entry.action) {
    case Action::file_open:
        spawn_with(launcher_config::open, entry.path);
        break;
    case Action::dir_open_file_manager:
        spawn_with(launcher_config::file_manager, entry.path);
        break;
    case Action::dir_open_editor:
        spawn_with(launcher_config::editor, entry.path);
        break;
    case Action::dir_open_terminal:
        spawn_detached("cd " + shell_quote(entry.path) + " && exec " + launcher_config::terminal);
        break;
    default:
        return;
    }
    visit_store_record(visits, visit_store_file_key(entry.path));
}

void launch_app(const DesktopEntry &entry, VisitStore &visits) {
    spawn_detached(app_command(entry));
    visit_store_record(visits, visit_store_app_key(entry.id));
}

} // namespace astralia
