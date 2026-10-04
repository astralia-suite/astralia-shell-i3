#include <algorithm>
#include <filesystem>

#include "config/bar_config.h"

#include "modules/bar/panel/tray_panel.h"

#include "render/icons.h"
#include "render/text.h"

namespace astralia {

namespace {

constexpr int columns = (bar_config::panel_width - 2 * panel_config::padding + bar_config::tray_grid_gap) / (bar_config::tray_cell_size + bar_config::tray_grid_gap);

std::string icon_path(const TrayItem &item) {
    if (item.icon_name.empty()) {
        return "";
    }
    if (!item.icon_theme_path.empty()) {
        std::error_code error;
        for (const char *extension : {".png", ".svg"}) {
            std::string candidate = item.icon_theme_path + "/" + item.icon_name + extension;
            if (std::filesystem::exists(candidate, error)) {
                return candidate;
            }
        }
    }
    return resolve_app_icon_path(item.icon_name);
}

void draw_glyph(cairo_t *cr, const char *glyph, double cx, double cy, const Color &color) {
    Text text(panel_config::icon_font);
    text.set(glyph);
    set_source(cr, color);
    text.draw_ink_centered(cr, cx, cy);
}

} // namespace

int tray_menu_height(const std::vector<TrayMenuEntry> *level, bool show_back) {
    int height = show_back || level == nullptr ? bar_config::tray_menu_row_height : 0;
    if (level != nullptr) {
        for (const TrayMenuEntry &entry : *level) {
            if (entry.visible) {
                height += entry.separator ? bar_config::tray_menu_separator_height : bar_config::tray_menu_row_height;
            }
        }
    }
    return 2 * bar_config::tray_menu_padding + height;
}

TrayMenu::TrayMenu(XConnection &x, EventLoop &loop, TrayService &tray)
    : x_(x), tray_(tray),
      window_(x, "astralia-tray-menu", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_LEAVE_WINDOW) {
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
}

const std::vector<TrayMenuEntry> *TrayMenu::level() const {
    const std::vector<TrayMenuEntry> *level = tray_.menu(key_);
    for (int32_t id : path_) {
        if (level == nullptr) {
            return nullptr;
        }
        auto it = std::ranges::find(*level, id, &TrayMenuEntry::id);
        if (it == level->end()) {
            return nullptr;
        }
        level = &it->children;
    }
    return level;
}

void TrayMenu::open(const std::string &key, int x, int y) {
    key_ = key;
    path_.clear();
    hovered_ = -1;
    anchor_x_ = x;
    anchor_y_ = y;
    open_ = true;
    tray_.request_menu(key);
    paint();
    window_.show(false);
    window_.present();
}

void TrayMenu::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    key_.clear();
    path_.clear();
    window_.hide();
    window_.release();
}

void TrayMenu::back() {
    if (path_.empty()) {
        close();
        return;
    }
    path_.pop_back();
    hovered_ = -1;
    paint();
}

void TrayMenu::refresh() {
    if (!open_) {
        return;
    }
    if (tray_.find(key_) == nullptr) {
        close();
        return;
    }
    paint();
}

void TrayMenu::handle(const xcb_generic_event_t &event) {
    if (!open_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        window_.present();
        break;
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        if (button.detail == XCB_BUTTON_INDEX_1 || button.detail == XCB_BUTTON_INDEX_3) {
            click(button.event_x, button.event_y);
        }
        break;
    }
    case XCB_MOTION_NOTIFY:
        hover(reinterpret_cast<const xcb_motion_notify_event_t &>(event).event_y);
        break;
    case XCB_LEAVE_NOTIFY:
        hover(std::nullopt);
        break;
    default:
        break;
    }
}

void TrayMenu::click(double x, double y) {
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    if (!hit) {
        return;
    }
    if (hit->action == go_back) {
        back();
        return;
    }
    const std::vector<TrayMenuEntry> *entries = level();
    if (entries == nullptr) {
        return;
    }
    int32_t id = std::stoi(hit->tag);
    auto it = std::ranges::find(*entries, id, &TrayMenuEntry::id);
    if (it == entries->end()) {
        return;
    }
    if (!it->children.empty()) {
        path_.push_back(id);
        hovered_ = -1;
        paint();
        return;
    }
    tray_.menu_clicked(key_, id);
    close();
}

void TrayMenu::hover(std::optional<double> y) {
    int hovered = -1;
    for (std::size_t i = 0; y && i < hits_.size(); ++i) {
        if (hits_[i].rect.contains(hits_[i].rect.x, *y)) {
            hovered = static_cast<int>(i);
        }
    }
    if (hovered != hovered_) {
        hovered_ = hovered;
        paint();
    }
}

void TrayMenu::paint() {
    constexpr int width = bar_config::tray_menu_width;
    constexpr double pad = bar_config::tray_menu_padding;
    constexpr double row_h = bar_config::tray_menu_row_height;
    constexpr double row_pad = bar_config::tray_menu_row_padding;
    const std::vector<TrayMenuEntry> *entries = level();
    bool show_back = !path_.empty();
    OutputGeometry output = x_.output_containing(anchor_x_, anchor_y_);
    int height = std::min(tray_menu_height(entries, show_back), static_cast<int>(output.height));
    int x = std::clamp(anchor_x_, static_cast<int>(output.x), output.x + output.width - width);
    int y = std::clamp(anchor_y_, static_cast<int>(output.y), output.y + output.height - height);
    window_.place({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<uint16_t>(width), static_cast<uint16_t>(height)});

    cairo_t *cr = window_.cr();
    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, height);
    double inner = width - 2 * pad;
    double row_y = pad;
    auto row = [&](int action, std::string tag) {
        PanelRect rect{pad, row_y, inner, row_h};
        if (static_cast<int>(hits_.size()) == hovered_) {
            set_source(cr, palette::text_alpha08);
            rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, metrics::radius_sm);
            cairo_fill(cr);
        }
        hits_.push_back({rect, action, std::move(tag)});
        row_y += row_h;
        return rect;
    };

    if (show_back) {
        PanelRect rect = row(go_back, {});
        draw_glyph(cr, icon::chevron_left, rect.x + row_pad + 6.0, rect.y + row_h / 2.0, palette::text);
        panel_draw_text(cr, panel_config::font, "Back", rect.x + 2 * row_pad + 12.0, rect.y, row_h, 0, palette::text);
    }
    if (entries == nullptr) {
        panel_draw_text(cr, panel_config::small_font, "Loading", pad + row_pad, row_y, row_h, 0, palette::text_dim);
    } else {
        for (const TrayMenuEntry &entry : *entries) {
            if (!entry.visible) {
                continue;
            }
            if (entry.separator) {
                set_source(cr, palette::text_alpha08);
                cairo_rectangle(cr, pad + row_pad, row_y + bar_config::tray_menu_separator_height / 2.0, inner - 2 * row_pad, 1.0);
                cairo_fill(cr);
                row_y += bar_config::tray_menu_separator_height;
                continue;
            }
            const Color &color = entry.enabled ? palette::text : palette::text_dim;
            PanelRect rect = entry.enabled ? row(menu_entry, std::to_string(entry.id)) : PanelRect{pad, row_y, inner, row_h};
            if (!entry.enabled) {
                row_y += row_h;
            }
            double text_x = rect.x + row_pad;
            if (entry.checkbox) {
                if (entry.checked) {
                    draw_glyph(cr, icon::check, text_x + 6.0, rect.y + row_h / 2.0, color);
                }
                text_x += 12.0 + row_pad;
            }
            double chevron = entry.children.empty() ? 0.0 : 12.0 + row_pad;
            panel_draw_text(cr, panel_config::font, entry.label, text_x, rect.y, row_h, static_cast<int>(rect.x + rect.w - row_pad - chevron - text_x), color);
            if (!entry.children.empty()) {
                draw_glyph(cr, icon::chevron_right, rect.x + rect.w - row_pad - 6.0, rect.y + row_h / 2.0, palette::text_dim);
            }
        }
    }
    window_.present();
}

TrayPanel::TrayPanel(XConnection &x, EventLoop &loop, TrayService &tray)
    : tray_(tray), menu_(x, loop, tray),
      window_(x, loop, "astralia-tray-panel", bar_config::panel_width, bar_config::panel_max_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] { menu_.close(); }) {
    tray_.changed.connect([this] {
        if (window_.is_open()) {
            paint();
        }
        menu_.refresh();
    });
}

void TrayPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    paint();
    window_.open(bar_config::margin_x, bar_config::panel_top);
}

void TrayPanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        if (window_.keyboard().press(key.detail, key.state).kind == KeyKind::escape) {
            if (menu_.is_open()) {
                menu_.back();
            } else {
                window_.close();
            }
        }
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        click(button.event_x, button.event_y, button.detail);
        break;
    }
    default:
        break;
    }
}

void TrayPanel::click(double x, double y, uint8_t button) {
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    if (!hit || (button != XCB_BUTTON_INDEX_1 && button != XCB_BUTTON_INDEX_3)) {
        menu_.close();
        return;
    }
    if (hit->action == close_panel) {
        window_.close();
        return;
    }
    const TrayItem *target = tray_.find(hit->tag);
    if (target == nullptr) {
        return;
    }
    if (button == XCB_BUTTON_INDEX_1) {
        menu_.close();
        tray_.activate(hit->tag);
        return;
    }
    if (target->menu_path.empty()) {
        menu_.close();
        return;
    }
    menu_.open(hit->tag, window_.x() + static_cast<int>(hit->rect.x), window_.y() + static_cast<int>(hit->rect.y + hit->rect.h) + bar_config::tray_menu_offset);
}

cairo_surface_t *TrayPanel::icon(const TrayItem &item) {
    std::string path = icon_path(item);
    if (path.empty()) {
        return nullptr;
    }
    auto it = icons_.find(path);
    if (it == icons_.end()) {
        it = icons_.emplace(path, load_app_icon(path, bar_config::tray_icon_size)).first;
    }
    return it->second.get();
}

void TrayPanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    constexpr double cell = bar_config::tray_cell_size;
    constexpr double gap = bar_config::tray_grid_gap;
    constexpr double size = bar_config::tray_icon_size;
    const std::vector<TrayItem> &items = tray_.items();
    double top = panel_content_top();
    int rows = (static_cast<int>(items.size()) + columns - 1) / columns;
    double content = items.empty() ? bar_config::panel_empty_height : rows * cell + (rows - 1) * gap;
    int height = static_cast<int>(top + content + pad);
    window_.set_height(height);

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, height);
    panel_draw_header(cr, width, "Tray", hits_, close_panel);
    if (items.empty()) {
        panel_draw_centered(cr, {pad, top, width - 2 * pad, content}, "No tray icons");
        window_.present();
        return;
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        PanelRect rect{pad + static_cast<double>(i % columns) * (cell + gap), top + static_cast<double>(i / columns) * (cell + gap), cell, cell};
        set_source(cr, palette::text_alpha06);
        rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, metrics::radius_md);
        cairo_fill(cr);
        if (cairo_surface_t *surface = icon(items[i])) {
            double scale = size / std::max(cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
            cairo_save(cr);
            cairo_translate(cr, rect.x + (cell - size) / 2.0, rect.y + (cell - size) / 2.0);
            cairo_scale(cr, scale, scale);
            cairo_set_source_surface(cr, surface, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
        } else {
            draw_glyph(cr, icon::apps, rect.x + cell / 2.0, rect.y + cell / 2.0, palette::text);
        }
        hits_.push_back({rect, item, items[i].key()});
    }
    window_.present();
}

} // namespace astralia
