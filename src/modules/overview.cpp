#include <algorithm>
#include <cmath>
#include <malloc.h>
#include <optional>

#include "config/overview_config.h"

#include "core/log.h"

#include "modules/overview.h"

#include "render/app_icon.h"
#include "render/draw.h"

namespace astralia {

namespace {

namespace cfg = overview_config;

void set_source_alpha(cairo_t *cr, const Color &color, double alpha) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, alpha);
}

void draw_box(cairo_t *cr, const OverviewRect &rect, double radius, const Color &fill, std::optional<Color> border, double border_width) {
    rounded_rect(cr, rect.x, rect.y, rect.width, rect.height, radius);
    set_source(cr, fill);
    cairo_fill(cr);
    if (!border) {
        return;
    }
    double inset = border_width / 2.0;
    rounded_rect(cr, rect.x + inset, rect.y + inset, rect.width - border_width, rect.height - border_width, std::max(0.0, radius - inset));
    set_source(cr, *border);
    cairo_set_line_width(cr, border_width);
    cairo_stroke(cr);
}

int icon_size_for(const OverviewRect &rect) {
    double wanted = std::min(rect.width, rect.height) * cfg::icon_to_tile_ratio;
    if (wanted < cfg::icon_min_size) {
        return 0;
    }
    int step = cfg::icon_size_step;
    int size = static_cast<int>(wanted) / step * step;
    return std::clamp(size, cfg::icon_min_size, cfg::icon_max_size);
}

} // namespace

Overview::Overview(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services)
    : x_(x), services_(services),
      window_(x, "astralia-overview", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE),
      keyboard_(x.conn()), number_(cfg::number_font) {
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
    services_.i3.windows_changed.connect([this] {
        if (open_) {
            refresh_tree();
            paint();
        }
    });
    services_.i3.changed.connect([this] {
        if (open_) {
            paint();
        }
    });
    ipc.add({"overview",
             [this] {
                 toggle();
                 return std::string();
             },
             "toggle the overview"});
}

void Overview::toggle() {
    if (open_) {
        close();
    } else {
        open();
    }
}

void Overview::open() {
    window_.place(x_.pointer_output());
    keyboard_.reload();
    refresh_tree();
    selected_ = services_.i3.status().current + 1;
    page_ = overview_page_of(selected_);
    dragging_ = false;
    open_ = true;
    compute_layout();
    paint();
    window_.show(true);
    log::info("overview: open");
}

void Overview::close() {
    window_.hide();
    open_ = false;
    dragging_ = false;
    tree_ = {};
    icons_.clear();
    window_.release();
    malloc_trim(0);
}

void Overview::refresh_tree() {
    tree_ = services_.i3.query_tree();
}

void Overview::expect_focus_loss() {
    focus_grace_until_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg::focus_grace_ms);
}

void Overview::compute_layout() {
    work_width_ = window_.geometry().width;
    work_height_ = window_.geometry().height;
    const I3Workspace *source = nullptr;
    for (const I3Workspace &workspace : tree_.workspaces) {
        if (source == nullptr || workspace.number == services_.i3.status().current + 1) {
            source = &workspace;
        }
    }
    if (source != nullptr && source->width > 0 && source->height > 0) {
        work_width_ = source->width;
        work_height_ = source->height;
    }
    layout_ = overview_layout(window_.geometry().width, window_.geometry().height, work_width_, work_height_, page_);
}

std::vector<Overview::Tile> Overview::tiles() const {
    std::vector<Tile> result;
    for (const I3Window &window : tree_.windows) {
        const OverviewCell *cell = overview_find_cell(layout_, window.workspace);
        if (cell == nullptr) {
            continue;
        }
        result.push_back({&window, overview_tile_rect(cell->rect, work_width_, work_height_, window.x, window.y, window.width, window.height)});
    }
    std::ranges::stable_sort(result, [](const Tile &a, const Tile &b) {
        if (a.window->floating != b.window->floating) {
            return !a.window->floating;
        }
        return a.window->fullscreen < b.window->fullscreen;
    });
    if (dragging_) {
        std::ranges::stable_partition(result, [this](const Tile &tile) { return tile.window->id != drag_id_; });
    }
    return result;
}

const Overview::Tile *Overview::tile_at(const std::vector<Tile> &tiles, double x, double y) const {
    for (auto it = tiles.rbegin(); it != tiles.rend(); ++it) {
        if (it->rect.contains(x, y)) {
            return &*it;
        }
    }
    return nullptr;
}

cairo_surface_t *Overview::icon_for(const std::string &window_class, int size) {
    std::string key = window_class + ":" + std::to_string(size);
    auto it = icons_.find(key);
    if (it == icons_.end()) {
        it = icons_.emplace(key, load_app_icon(resolve_window_icon_path(window_class), size)).first;
    }
    return it->second.get();
}

void Overview::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        if (open_) {
            window_.present();
        }
        break;
    case XCB_KEY_PRESS:
        if (open_) {
            const auto &pressed = reinterpret_cast<const xcb_key_press_event_t &>(event);
            key(keyboard_.press(pressed.detail, pressed.state));
        }
        break;
    case XCB_BUTTON_PRESS: {
        const auto &pressed = reinterpret_cast<const xcb_button_press_event_t &>(event);
        if (open_ && pressed.detail == XCB_BUTTON_INDEX_1) {
            press(pressed.event_x, pressed.event_y);
        }
        break;
    }
    case XCB_BUTTON_RELEASE: {
        const auto &released = reinterpret_cast<const xcb_button_release_event_t &>(event);
        if (open_ && released.detail == XCB_BUTTON_INDEX_1) {
            release();
        }
        break;
    }
    case XCB_MOTION_NOTIFY: {
        const auto &motion = reinterpret_cast<const xcb_motion_notify_event_t &>(event);
        if (open_ && dragging_) {
            move(motion.event_x, motion.event_y);
        }
        break;
    }
    case XCB_FOCUS_OUT: {
        const auto &focus = reinterpret_cast<const xcb_focus_out_event_t &>(event);
        if (!open_ || focus.mode != XCB_NOTIFY_MODE_NORMAL || focus.detail == XCB_NOTIFY_DETAIL_POINTER) {
            break;
        }
        if (std::chrono::steady_clock::now() < focus_grace_until_) {
            window_.focus();
        } else {
            close();
        }
        break;
    }
    default:
        break;
    }
}

void Overview::select(uint32_t workspace) {
    selected_ = workspace;
    page_ = overview_page_of(workspace);
    expect_focus_loss();
    services_.i3.switch_to(workspace - 1);
    compute_layout();
}

void Overview::key(const KeyEvent &event) {
    switch (event.kind) {
    case KeyKind::left:
        select(overview_step(selected_, -1, 0));
        break;
    case KeyKind::right:
        select(overview_step(selected_, 1, 0));
        break;
    case KeyKind::up:
        select(overview_step(selected_, 0, -1));
        break;
    case KeyKind::down:
        select(overview_step(selected_, 0, 1));
        break;
    case KeyKind::escape:
        close();
        return;
    case KeyKind::text: {
        if (event.text.size() != 1) {
            return;
        }
        char c = event.text[0];
        if (c >= '0' && c <= '9') {
            uint32_t position = c == '0' ? 10 : static_cast<uint32_t>(c - '0');
            if (position > cfg::per_page) {
                return;
            }
            select(page_ * cfg::per_page + position);
        } else if (c == 'd') {
            expect_focus_loss();
            for (const I3Window &window : tree_.windows) {
                if (window.workspace == selected_) {
                    services_.i3.kill_window(window.id);
                }
            }
            refresh_tree();
        } else {
            return;
        }
        break;
    }
    default:
        return;
    }
    paint();
}

void Overview::press(double x, double y) {
    std::vector<Tile> visible = tiles();
    if (const Tile *tile = tile_at(visible, x, y)) {
        dragging_ = true;
        drag_id_ = tile->window->id;
        drag_from_ = tile->window->workspace;
        drag_dx_ = x - tile->rect.x;
        drag_dy_ = y - tile->rect.y;
        pointer_x_ = x;
        pointer_y_ = y;
        paint();
        return;
    }
    if (const OverviewCell *cell = overview_cell_at(layout_, x, y)) {
        select(cell->workspace);
        paint();
        return;
    }
    OverviewRect panel = layout_.panel;
    constexpr double margin = cfg::screen_margin;
    if (!OverviewRect{panel.x - margin, panel.y - margin, panel.width + 2 * margin, panel.height + 2 * margin}.contains(x, y)) {
        close();
    }
}

void Overview::move(double x, double y) {
    if (x == pointer_x_ && y == pointer_y_) {
        return;
    }
    pointer_x_ = x;
    pointer_y_ = y;
    paint();
}

void Overview::release() {
    if (!dragging_) {
        return;
    }
    dragging_ = false;
    if (const OverviewCell *cell = overview_cell_at(layout_, pointer_x_, pointer_y_)) {
        if (cell->workspace != drag_from_) {
            expect_focus_loss();
            services_.i3.move_window(drag_id_, cell->workspace);
            refresh_tree();
        } else {
            select(cell->workspace);
        }
    }
    paint();
}

void Overview::paint() {
    cairo_t *cr = window_.cr();
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double scale = layout_.scale;
    draw_box(cr, layout_.panel, cfg::screen_rounding * scale + cfg::padding, palette::field_bg, palette::accent, cfg::background_border_width);

    std::optional<uint32_t> drag_target;
    if (dragging_) {
        if (const OverviewCell *cell = overview_cell_at(layout_, pointer_x_, pointer_y_)) {
            drag_target = cell->workspace;
        }
    }

    for (const OverviewCell &cell : layout_.cells) {
        draw_box(cr, cell.rect, cfg::screen_rounding * scale, palette::field_bg, std::nullopt, 0.0);
        rounded_rect(cr, cell.rect.x + cfg::workspace_border_width / 2.0, cell.rect.y + cfg::workspace_border_width / 2.0, cell.rect.width - cfg::workspace_border_width, cell.rect.height - cfg::workspace_border_width, cfg::screen_rounding * scale);
        set_source(cr, drag_target == cell.workspace ? palette::text_alpha08 : palette::text_alpha20);
        cairo_set_line_width(cr, cfg::workspace_border_width);
        cairo_stroke(cr);

        number_.set(std::to_string(cell.workspace));
        if (number_.width() <= cell.rect.width && number_.height() <= cell.rect.height) {
            set_source_alpha(cr, palette::text, 1.0 - cfg::number_fade);
            number_.draw_ink_centered(cr, cell.rect.x + cell.rect.width / 2.0, cell.rect.y + cell.rect.height / 2.0);
        }
    }

    for (const Tile &tile : tiles()) {
        OverviewRect rect = tile.rect;
        if (dragging_ && tile.window->id == drag_id_) {
            rect.x = pointer_x_ - drag_dx_;
            rect.y = pointer_y_ - drag_dy_;
        }
        draw_box(cr, rect, cfg::window_rounding * scale, palette::surface_alt, palette::accent, cfg::window_border_width);
        int size = icon_size_for(rect);
        if (size == 0) {
            continue;
        }
        if (cairo_surface_t *icon = icon_for(tile.window->window_class, size)) {
            int w = cairo_image_surface_get_width(icon);
            int h = cairo_image_surface_get_height(icon);
            cairo_set_source_surface(cr, icon, std::round(rect.x + (rect.width - w) / 2.0), std::round(rect.y + (rect.height - h) / 2.0));
            cairo_paint(cr);
        }
    }

    if (const OverviewCell *active = overview_find_cell(layout_, selected_)) {
        double inset = cfg::indicator_border_width / 2.0;
        rounded_rect(cr, active->rect.x + inset, active->rect.y + inset, active->rect.width - cfg::indicator_border_width, active->rect.height - cfg::indicator_border_width, cfg::screen_rounding * scale);
        set_source(cr, palette::accent_alt);
        cairo_set_line_width(cr, cfg::indicator_border_width);
        cairo_stroke(cr);
    }
    window_.present();
}

} // namespace astralia
