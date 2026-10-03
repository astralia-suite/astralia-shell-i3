#include "modules/launcher/submenu.h"
#include "modules/launcher/files_provider.h"

#include "render/icons.h"

namespace astralia {

namespace {

using Action = SubmenuEntry::Action;

void append_listing(SubmenuState &s, const DirLister &list_dir, const std::string &path,
                    bool dirs) {
    for (const FileEntry &file : list_dir(path, dirs)) {
        s.items.push_back({file.name, file.path, dirs, nullptr, Action::none});
    }
}

} // namespace

void submenu_close(SubmenuState &s) {
    s.screen = SubmenuScreen::search;
    s.current_path.clear();
    s.came_from_browse = false;
    s.items.clear();
}

void submenu_open_directory(SubmenuState &s, const std::string &path, const DirLister &list_dir) {
    s.screen = SubmenuScreen::browse;
    s.current_path = path;
    s.items = {
        {"Open Directory", path, true, icon::arrow_right, Action::open_options},
        {"Previous Directory", path, true, icon::arrow_left, Action::prev_dir},
    };
    append_listing(s, list_dir, path, true);
    append_listing(s, list_dir, path, false);
}

void submenu_open_directory_actions(SubmenuState &s, const std::string &path) {
    s.screen = SubmenuScreen::dir_actions;
    s.current_path = path;
    s.items = {
        {"Open Directory in File Manager", path, true, icon::arrow_right,
         Action::dir_open_file_manager},
        {"Open Directory in Editor", path, true, icon::code, Action::dir_open_editor},
        {"Open Directory in Terminal", path, true, icon::terminal, Action::dir_open_terminal},
    };
}

void submenu_open_file_actions(SubmenuState &s, const std::string &path) {
    s.came_from_browse = s.screen == SubmenuScreen::browse;
    s.screen = SubmenuScreen::file_actions;
    s.current_path = path;
    s.items = {
        {"Open File", path, false, icon::arrow_right, Action::file_open},
        {"Open Containing Directory", parent_of(path), true, icon::folder_open,
         Action::open_containing_dir},
    };
}

bool submenu_handle_entry(SubmenuState &s, const SubmenuEntry &entry, const DirLister &list_dir) {
    switch (entry.action) {
    case Action::open_options:
    case Action::open_containing_dir:
        submenu_open_directory_actions(s, entry.path);
        return true;
    case Action::none:
        if (entry.is_dir) {
            submenu_open_directory(s, entry.path, list_dir);
        } else {
            submenu_open_file_actions(s, entry.path);
        }
        return true;
    case Action::prev_dir:
        if (s.current_path != "/") {
            submenu_open_directory(s, parent_of(s.current_path), list_dir);
        }
        return true;
    default:
        return false;
    }
}

bool submenu_go_back(SubmenuState &s, const DirLister &list_dir) {
    switch (s.screen) {
    case SubmenuScreen::dir_actions:
        submenu_open_directory(s, s.current_path, list_dir);
        return true;
    case SubmenuScreen::file_actions:
        if (s.came_from_browse) {
            submenu_open_directory(s, parent_of(s.current_path), list_dir);
        } else {
            submenu_close(s);
        }
        return true;
    case SubmenuScreen::browse:
        submenu_close(s);
        return true;
    case SubmenuScreen::search:
        return false;
    }
    return false;
}

} // namespace astralia
