#include <algorithm>
#include <cstdlib>
#include <malloc.h>
#include <sys/timerfd.h>

#include "core/log.h"

#include "modules/launcher.h"
#include "modules/launcher/apps_provider.h"
#include "modules/launcher/desktop_entry.h"
#include "modules/launcher/files_provider.h"
#include "modules/launcher/launch_action.h"
#include "modules/launcher/search.h"
#include "modules/launcher/submenu.h"
#include "modules/launcher/visit_store.h"

#include "render/draw.h"
#include "render/icons.h"

namespace astralia {

namespace {

namespace cfg = launcher_config;

void fill_rounded(cairo_t *cr, double x, double y, double w, double h, double r,
                  const Color &color) {
    rounded_rect(cr, x, y, w, h, r);
    set_source(cr, color);
    cairo_fill(cr);
}

void stroke_rounded(cairo_t *cr, double x, double y, double w, double h, double r, double line,
                    const Color &color) {
    double inset = line / 2.0;
    rounded_rect(cr, x + inset, y + inset, w - line, h - line, r - inset);
    set_source(cr, color);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);
}

const char *mode_icon(LauncherMode mode) {
    switch (mode) {
    case LauncherMode::run:
        return icon::terminal;
    case LauncherMode::google:
        return icon::brand_google;
    case LauncherMode::youtube:
        return icon::brand_youtube;
    case LauncherMode::duckduckgo:
    case LauncherMode::url:
        return icon::link;
    case LauncherMode::drun:
        return icon::apps;
    }
    return icon::apps;
}

void pop_utf8(std::string &text) {
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

IconSurface load_bullet(int number) {
    std::string name = cfg::bullet_prefix + std::to_string(number) + cfg::bullet_suffix;
    for (const char *dir : {ASTRALIA_CONSTELLATION_DIR, ASTRALIA_SOURCE_CONSTELLATION_DIR}) {
        IconSurface surface(
            cairo_image_surface_create_from_png((std::string(dir) + "/" + name).c_str()));
        if (cairo_surface_status(surface.get()) == CAIRO_STATUS_SUCCESS) {
            return surface;
        }
    }
    log::error("launcher: cannot load bullet {}", name);
    return nullptr;
}

int surface_width(cairo_surface_t *surface) { return cairo_image_surface_get_width(surface); }

int surface_height(cairo_surface_t *surface) { return cairo_image_surface_get_height(surface); }

double content_height(int visible_rows) {
    double h = cfg::menu_pad * 2.0 + cfg::search_height;
    if (visible_rows > 0) {
        h += cfg::list_gap + visible_rows * cfg::row_height + (visible_rows - 1) * cfg::row_spacing;
    }
    return h;
}

} // namespace

Launcher::Launcher(XConnection &x, EventLoop &loop, IpcServer &ipc)
    : x_(x), loop_(loop), window_(x, "astralia-launcher", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE),
      keyboard_(x.conn()), text_(cfg::font), small_text_(cfg::small_font),
      glyph_(cfg::icon_font) {
    const char *home = getenv("HOME");
    home_ = home != nullptr ? home : "";
    visits_ = visit_store_load(visit_store_path(getenv("XDG_STATE_HOME"), home));
    for (int i = 0; i < cfg::max_visible; ++i) {
        bullets_[i] = load_bullet(i + 1);
    }

    debounce_ = UniqueFd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC));
    loop_.on_fd(debounce_.get(), [this] { debounce_fired(); });
    loop_.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
    ipc.add({"launcher",
             [this] {
                 toggle(false);
                 return std::string();
             },
             "toggle the launcher, searching from $HOME"});
    ipc.add({"launcher global",
             [this] {
                 toggle(true);
                 return std::string();
             },
             "toggle the launcher, searching from /"});
}

Launcher::~Launcher() {
    stop_search();
    loop_.remove_fd(debounce_.get());
}

void Launcher::toggle(bool global) {
    if (open_) {
        close();
    } else {
        open(global);
    }
}

void Launcher::open(bool global) {
    window_.place(x_.pointer_output());
    search_root_ = global || home_.empty() ? "/" : home_;
    apps_ = scan_desktop_entries();
    keyboard_.reload();
    open_ = true;
    paint();
    window_.show(true);
    log::info("launcher: open, searching from {}", search_root_);
}

void Launcher::close() {
    stop_search();
    itimerspec disarm{};
    timerfd_settime(debounce_.get(), 0, &disarm, nullptr);
    window_.hide();
    open_ = false;
    query_.clear();
    mode_ = LauncherMode::drun;
    effective_query_.clear();
    results_.clear();
    submenu_close(submenu_);
    selected_ = -1;
    hovered_ = -1;
    hits_.clear();
    malloc_trim(0);
}

void Launcher::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        if (open_) {
            window_.present();
        }
        break;
    case XCB_KEY_PRESS:
        if (open_) {
            const auto &press = reinterpret_cast<const xcb_key_press_event_t &>(event);
            key(keyboard_.press(press.detail, press.state));
        }
        break;
    case XCB_BUTTON_PRESS: {
        const auto &press = reinterpret_cast<const xcb_button_press_event_t &>(event);
        if (open_ && press.detail == XCB_BUTTON_INDEX_1) {
            click(press.event_x, press.event_y);
        }
        break;
    }
    case XCB_MOTION_NOTIFY: {
        const auto &motion = reinterpret_cast<const xcb_motion_notify_event_t &>(event);
        hover(row_at(motion.event_x, motion.event_y));
        break;
    }
    case XCB_LEAVE_NOTIFY:
        hover(std::nullopt);
        break;
    case XCB_FOCUS_OUT: {
        const auto &focus = reinterpret_cast<const xcb_focus_out_event_t &>(event);
        if (open_ && focus.mode == XCB_NOTIFY_MODE_NORMAL &&
            focus.detail != XCB_NOTIFY_DETAIL_POINTER) {
            close();
        }
        break;
    }
    default:
        break;
    }
}

void Launcher::key(const KeyEvent &event) {
    hovered_ = -1;
    switch (event.kind) {
    case KeyKind::text:
    case KeyKind::backspace:
        if (submenu_.screen != SubmenuScreen::search) {
            submenu_close(submenu_);
        }
        if (event.kind == KeyKind::text) {
            query_ += event.text;
        } else {
            pop_utf8(query_);
        }
        query_changed();
        break;
    case KeyKind::up:
    case KeyKind::down: {
        int count = item_count();
        selected_ = count == 0 ? -1
                               : std::clamp(selected_ + (event.kind == KeyKind::down ? 1 : -1), 0,
                                            count - 1);
        break;
    }
    case KeyKind::escape:
        if (submenu_.screen == SubmenuScreen::search) {
            close();
            return;
        }
        submenu_go_back(submenu_, list_directory);
        reset_selection();
        break;
    case KeyKind::enter:
        launch_selected();
        break;
    case KeyKind::left:
    case KeyKind::right:
    case KeyKind::none:
        return;
    }
    if (open_) {
        paint();
    }
}

void Launcher::click(double x, double y) {
    if (std::optional<int> row = row_at(x, y)) {
        selected_ = *row;
        hovered_ = -1;
        launch_selected();
        if (open_) {
            paint();
        }
    } else if (!box_.contains(x, y)) {
        close();
    }
}

void Launcher::hover(std::optional<int> row) {
    int next = open_ && row ? *row : -1;
    if (next != hovered_) {
        hovered_ = next;
        if (open_) {
            paint();
        }
    }
}

void Launcher::query_changed() {
    ModeQuery mq = detect_mode_and_query(query_);
    mode_ = mq.mode;
    effective_query_ = mq.query;
    itimerspec spec{};
    spec.it_value.tv_nsec = cfg::debounce_ms * 1'000'000L;
    timerfd_settime(debounce_.get(), 0, &spec, nullptr);
}

void Launcher::debounce_fired() {
    uint64_t expirations = 0;
    if (read(debounce_.get(), &expirations, sizeof expirations) <= 0 || !open_) {
        return;
    }
    if (mode_ == LauncherMode::drun && !effective_query_.empty()) {
        start_search();
        return;
    }
    stop_search();
    results_.clear();
    if (submenu_.screen == SubmenuScreen::search) {
        selected_ = -1;
    }
    paint();
}

void Launcher::start_search() {
    stop_search();
    search_query_ = effective_query_;
    std::string pattern = to_glob_pattern(search_query_);
    searching_ = true;
    for (bool dirs : {true, false}) {
        SearchProcess &process = dirs ? dirs_ : files_;
        if (process.start(fd_search_argv(pattern, search_root_, dirs, cfg::max_results))) {
            loop_.on_fd(process.fd(), [this, &process] { search_output(process); });
        }
    }
    if (dirs_.fd() < 0 && files_.fd() < 0) {
        search_done();
    }
}

void Launcher::stop_search() {
    for (SearchProcess *process : {&dirs_, &files_}) {
        if (process->fd() >= 0) {
            loop_.remove_fd(process->fd());
        }
        process->cancel();
    }
    searching_ = false;
}

void Launcher::search_output(SearchProcess &process) {
    int fd = process.fd();
    if (!process.read_available()) {
        return;
    }
    loop_.remove_fd(fd);
    if (searching_ && dirs_.fd() < 0 && files_.fd() < 0) {
        search_done();
    }
}

void Launcher::search_done() {
    searching_ = false;
    std::vector<ScoredApp> apps = search_apps(apps_, search_query_);
    std::vector<FileEntry> files;
    for (bool dirs : {true, false}) {
        for (FileEntry &file : fd_search_parse_output((dirs ? dirs_ : files_).output(), dirs)) {
            file.score = score_path(file.name, search_query_);
            if (file.score >= 0.0f) {
                files.push_back(std::move(file));
            }
        }
    }
    results_ = combined_drun_results(apps, files, visits_, cfg::max_results);
    if (submenu_.screen == SubmenuScreen::search) {
        selected_ = results_.empty() ? -1 : 0;
    }
    if (open_) {
        paint();
    }
}

void Launcher::launch_selected() {
    if (submenu_.screen != SubmenuScreen::search) {
        if (selected_ < 0 || selected_ >= static_cast<int>(submenu_.items.size())) {
            return;
        }
        SubmenuEntry entry = submenu_.items[selected_];
        if (submenu_handle_entry(submenu_, entry, list_directory)) {
            reset_selection();
            return;
        }
        launch_submenu_action(entry, visits_);
        close();
        return;
    }
    if (mode_ != LauncherMode::drun) {
        launch_non_drun(mode_, effective_query_);
        close();
        return;
    }
    if (selected_ < 0 || selected_ >= static_cast<int>(results_.size())) {
        return;
    }
    const DrunResult &result = results_[selected_];
    switch (result.kind) {
    case DrunResult::Kind::app:
        launch_app(*result.app, visits_);
        close();
        break;
    case DrunResult::Kind::dir:
        submenu_open_directory(submenu_, result.file.path, list_directory);
        reset_selection();
        break;
    case DrunResult::Kind::file:
        submenu_open_file_actions(submenu_, result.file.path);
        reset_selection();
        break;
    }
}

void Launcher::reset_selection() {
    selected_ = item_count() == 0 ? -1 : 0;
    hovered_ = -1;
}

int Launcher::item_count() const {
    return static_cast<int>(submenu_.screen == SubmenuScreen::search ? results_.size()
                                                                     : submenu_.items.size());
}

int Launcher::first_visible() const {
    return selected_ >= cfg::max_visible ? selected_ - cfg::max_visible + 1 : 0;
}

std::vector<Launcher::Row> Launcher::rows() {
    std::vector<Row> out;
    if (submenu_.screen == SubmenuScreen::search) {
        for (const DrunResult &result : results_) {
            switch (result.kind) {
            case DrunResult::Kind::app:
                out.push_back({icon::apps, result.app->name, "", app_icon(*result.app)});
                break;
            case DrunResult::Kind::dir:
                out.push_back(
                    {icon::folder, result.file.name, collapse_home(result.file.path, home_)});
                break;
            case DrunResult::Kind::file:
                out.push_back(
                    {icon::edit, result.file.name, collapse_home(result.file.path, home_)});
                break;
            }
        }
        return out;
    }
    for (const SubmenuEntry &entry : submenu_.items) {
        const char *glyph =
            entry.icon != nullptr ? entry.icon : (entry.is_dir ? icon::folder : icon::edit);
        out.push_back(
            {glyph, entry.name, entry.path.empty() ? "" : collapse_home(entry.path, home_)});
    }
    return out;
}

cairo_surface_t *Launcher::app_icon(const DesktopEntry &entry) {
    auto it = app_icons_.find(entry.id);
    if (it == app_icons_.end()) {
        it =
            app_icons_
                .emplace(entry.id, load_app_icon(resolve_app_icon_path(entry.icon), cfg::icon_size))
                .first;
    }
    return it->second.get();
}

std::optional<int> Launcher::row_at(double x, double y) const {
    for (const Hit &hit : hits_) {
        if (hit.box.contains(x, y)) {
            return hit.index;
        }
    }
    return std::nullopt;
}

void Launcher::paint() {
    std::vector<Row> all = rows();
    int first = first_visible();
    int visible = std::min(static_cast<int>(all.size()), cfg::max_visible);
    double box_h = content_height(visible);
    double box_x = (window_.geometry().width - cfg::width) / 2.0;
    double box_y = (window_.geometry().height - box_h) / 2.0;
    box_ = {box_x, box_y, static_cast<double>(cfg::width), box_h};
    hits_.clear();

    cairo_t *cr = window_.cr();
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    fill_rounded(cr, box_x, box_y, cfg::width, box_h, metrics::radius_md, palette::base_alpha80);
    stroke_rounded(cr, box_x, box_y, cfg::width, box_h, metrics::radius_md, cfg::menu_border_width,
                   palette::accent);

    double mode_x = box_x + cfg::menu_pad;
    double field_top = box_y + cfg::menu_pad;
    double center_y = field_top + cfg::search_height / 2.0;
    stroke_rounded(cr, mode_x, field_top, cfg::search_height, cfg::search_height,
                   metrics::radius_sm, cfg::border_width, palette::accent);
    set_source(cr, palette::text);
    glyph_.set(mode_icon(mode_));
    glyph_.draw(cr, mode_x + (cfg::search_height - glyph_.width()) / 2.0,
                center_y - glyph_.height() / 2.0);

    double field_x = mode_x + cfg::search_height + cfg::pad;
    double field_w = box_x + cfg::width - cfg::menu_pad - field_x;
    stroke_rounded(cr, field_x, field_top, field_w, cfg::search_height, metrics::radius_sm,
                   cfg::border_width, palette::accent);
    double text_x = field_x + cfg::pad;
    set_source(cr, palette::text);
    text_.set(elide(query_, cfg::max_row_chars));
    double caret_x = text_x;
    if (!query_.empty()) {
        text_.draw(cr, text_x, center_y - text_.height() / 2.0);
        caret_x += text_.width();
    }
    double caret_h = cfg::search_height - 2.0 * cfg::pad;
    cairo_rectangle(cr, caret_x, center_y - caret_h / 2.0, cfg::caret_width, caret_h);
    cairo_fill(cr);

    if (visible > 0) {
        double content_x = mode_x + cfg::search_height + cfg::bullet_gap;
        double row_w = box_x + cfg::width - cfg::menu_pad - content_x;
        double list_top = box_y + cfg::list_top;
        double list_h = visible * cfg::row_height + (visible - 1) * cfg::row_spacing;
        cairo_save(cr);
        cairo_rectangle(cr, mode_x, list_top, cfg::width - 2 * cfg::menu_pad, list_h);
        cairo_clip(cr);

        for (int slot = 0; slot < visible; ++slot) {
            int i = first + slot;
            const Row &row = all[i];
            double y = list_top + slot * cfg::row_pitch;
            hits_.push_back({{content_x, y, row_w, cfg::row_height}, i});
            fill_rounded(cr, content_x, y, row_w, cfg::row_height, metrics::radius_sm,
                         palette::text_alpha03);
            if (i == hovered_ && i != selected_) {
                stroke_rounded(cr, content_x, y, row_w, cfg::row_height, metrics::radius_sm,
                               cfg::border_width, palette::accent);
            }

            double row_x = content_x + cfg::pad;
            if (row.icon != nullptr) {
                int w = surface_width(row.icon);
                int h = surface_height(row.icon);
                cairo_set_source_surface(cr, row.icon, row_x + (cfg::icon_size - w) / 2.0,
                                         y + (cfg::row_height - h) / 2.0);
                cairo_paint(cr);
            } else {
                set_source(cr, palette::text);
                glyph_.set(row.glyph);
                glyph_.draw(cr, row_x + (cfg::icon_size - glyph_.width()) / 2.0,
                            y + (cfg::row_height - glyph_.height()) / 2.0);
            }
            row_x += cfg::icon_size + cfg::pad;

            text_.set(elide(row.label, cfg::max_row_chars));
            set_source(cr, palette::text);
            if (row.subtitle.empty()) {
                text_.draw(cr, row_x, y + (cfg::row_height - text_.height()) / 2.0);
                continue;
            }
            small_text_.set(elide_middle(row.subtitle, cfg::max_row_chars));
            double top =
                y +
                (cfg::row_height - text_.height() - cfg::two_line_gap - small_text_.height()) / 2.0;
            text_.draw(cr, row_x, top);
            set_source(cr, palette::text_alpha65);
            small_text_.draw(cr, row_x, top + text_.height() + cfg::two_line_gap);
        }

        for (int slot = 0; slot < visible; ++slot) {
            cairo_surface_t *bullet = bullets_[slot].get();
            if (bullet == nullptr) {
                continue;
            }
            cairo_save(cr);
            cairo_translate(cr, mode_x + (cfg::search_height - cfg::bullet_size) / 2.0,
                            list_top + slot * cfg::row_pitch +
                                (cfg::row_height - cfg::bullet_size) / 2.0);
            cairo_scale(cr, cfg::bullet_size / surface_width(bullet),
                        cfg::bullet_size / surface_height(bullet));
            cairo_set_source_surface(cr, bullet, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
        }

        if (selected_ >= 0) {
            stroke_rounded(cr, content_x, list_top + (selected_ - first) * cfg::row_pitch, row_w,
                           cfg::row_height, metrics::radius_sm, cfg::highlight_border_width,
                           palette::accent_alt_alpha50);
        }
        cairo_restore(cr);
    }
    window_.present();
}

} // namespace astralia
