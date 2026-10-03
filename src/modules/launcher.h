#pragma once

#include <array>
#include <cairo.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>

#include "config/launcher_config.h"

#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/keyboard.h"
#include "core/unique_fd.h"
#include "core/x_connection.h"

#include "modules/launcher/app_icon.h"
#include "modules/launcher/search_process.h"

#include "render/app_fonts.h"
#include "render/text.h"
#include "render/x_window.h"

namespace astralia {

class Launcher {
  public:
    Launcher(XConnection &x, EventLoop &loop, IpcServer &ipc);
    ~Launcher();

  private:
    struct Row {
        const char *glyph;
        std::string label;
        std::string subtitle;
        cairo_surface_t *icon = nullptr;
    };

    struct Box {
        double x;
        double y;
        double width;
        double height;

        bool contains(double px, double py) const {
            return px >= x && px < x + width && py >= y && py < y + height;
        }
    };

    struct Hit {
        Box box;
        int index;
    };

    void toggle(bool global);
    void open(bool global);
    void close();
    void handle(const xcb_generic_event_t &event);
    void key(const KeyEvent &event);
    void click(double x, double y);
    void hover(std::optional<int> row);
    void query_changed();
    void debounce_fired();
    void start_search();
    void stop_search();
    void search_output(SearchProcess &process);
    void search_done();
    void launch_selected();
    void reset_selection();
    int item_count() const;
    int first_visible() const;
    std::vector<Row> rows();
    cairo_surface_t *app_icon(const DesktopEntry &entry);
    std::optional<int> row_at(double x, double y) const;
    void paint();

    XConnection &x_;
    EventLoop &loop_;
    XWindow window_;
    Keyboard keyboard_;
    bool fonts_ = register_app_fonts();
    Text text_;
    Text small_text_;
    Text glyph_;
    std::array<IconSurface, launcher_config::max_visible> bullets_;
    std::unordered_map<std::string, IconSurface> app_icons_;
    UniqueFd debounce_;
    SearchProcess dirs_;
    SearchProcess files_;
    bool searching_ = false;
    bool open_ = false;
    std::string home_;
    std::string search_root_;
    std::string query_;
    LauncherMode mode_ = LauncherMode::drun;
    std::string effective_query_;
    std::string search_query_;
    std::vector<DesktopEntry> apps_;
    std::vector<DrunResult> results_;
    SubmenuState submenu_;
    VisitStore visits_;
    int selected_ = -1;
    int hovered_ = -1;
    std::vector<Hit> hits_;
    Box box_{};
};

} // namespace astralia
