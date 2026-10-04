#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>

#include "config/settings_config.h"
#include "config/wallpaper_config.h"

#include "core/config_file.h"

#include "modules/settings/layout.h"
#include "modules/settings/wallpaper_tab.h"
#include "modules/settings/widgets.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool is_image(const std::filesystem::path &path) {
    std::string extension = lowered(path.extension().string());
    return std::ranges::find(cfg::image_extensions, extension) != cfg::image_extensions.end();
}

void pop_character(std::string &text) {
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

} // namespace

WallpaperTab::WallpaperTab(Services &services, EventLoop &loop, std::function<void()> changed)
    : services_(services), selected_(wallpaper_config::any_output), thumbnails_(loop, std::move(changed)) {}

void WallpaperTab::open() {
    scanned_dir_.clear();
    editing_ = false;
    thumbnails_.start();
    refresh();
}

void WallpaperTab::close() {
    editing_ = false;
    images_.clear();
    scanned_dir_.clear();
    thumbnails_.stop();
}

void WallpaperTab::refresh() {
    if (services_.settings.wallpaper_dir() != scanned_dir_) {
        scan();
    }
}

void WallpaperTab::scan() {
    scanned_dir_ = services_.settings.wallpaper_dir();
    const char *home = std::getenv("HOME");
    std::filesystem::path dir(expand_home(scanned_dir_, home != nullptr ? home : ""));
    images_.clear();
    thumbnails_.clear();
    scroll_ = 0.0;
    std::error_code error;
    std::filesystem::directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && it != std::filesystem::directory_iterator(); it.increment(error)) {
        if (it->is_regular_file(error) && is_image(it->path())) {
            images_.push_back(it->path().string());
        }
    }
    std::ranges::sort(images_, [](const std::string &a, const std::string &b) { return lowered(a) < lowered(b); });
    if (images_.size() > cfg::max_images) {
        images_.resize(cfg::max_images);
    }
}

void WallpaperTab::paint(cairo_t *cr, const PanelRect &area, std::vector<PanelHit> &hits) {
    const std::vector<Output> &outputs = services_.outputs.outputs();
    if (std::ranges::none_of(outputs, [&](const Output &output) { return !output.name.empty() && output.name == selected_; })) {
        auto named = std::ranges::find_if(outputs, [](const Output &output) { return !output.name.empty(); });
        selected_ = named != outputs.end() ? named->name : wallpaper_config::any_output;
    }
    settings_draw_chips(cr, area, std::nullopt, outputs, selected_, chip, hits);

    double bar_y = area.y + cfg::chip_height + cfg::section_gap;
    panel_draw_text(cr, panel_config::font, cfg::folder_label, area.x, bar_y, cfg::folder_bar_height, 0, palette::text_muted);
    PanelRect field{area.x + cfg::folder_label_width, bar_y + (cfg::folder_bar_height - cfg::folder_field_height) / 2.0,
                    area.w - cfg::folder_label_width - cfg::folder_gap - cfg::folder_button_width, cfg::folder_field_height};
    set_source(cr, palette::field_bg);
    rounded_rect(cr, field.x, field.y, field.w, field.h, metrics::radius_sm);
    cairo_fill(cr);
    set_source(cr, editing_ ? palette::accent : palette::lavender_alpha20);
    cairo_set_line_width(cr, panel_config::border_width);
    rounded_rect(cr, field.x + 1, field.y + 1, field.w - 2, field.h - 2, metrics::radius_sm);
    cairo_stroke(cr);
    std::string shown = editing_ ? buffer_ + "|" : services_.settings.wallpaper_dir();
    panel_draw_text(cr, panel_config::font, shown, field.x + 8, field.y, field.h, static_cast<int>(field.w - 16), palette::text);
    hits.push_back({field, folder_field, {}});

    PanelRect button{field.x + field.w + cfg::folder_gap, field.y, cfg::folder_button_width, field.h};
    set_source(cr, palette::text_alpha08);
    rounded_rect(cr, button.x, button.y, button.w, button.h, metrics::radius_sm);
    cairo_fill(cr);
    panel_draw_centered(cr, button, cfg::reset_label);
    hits.push_back({button, reset, {}});

    double grid_y = bar_y + cfg::folder_bar_height + cfg::section_gap;
    double grid_w = settings_grid_width();
    grid_ = {area.x + std::max(0.0, (area.w - grid_w) / 2.0), grid_y, grid_w, area.y + area.h - grid_y};
    std::vector<std::string> wanted;
    if (images_.empty()) {
        panel_draw_centered(cr, grid_, cfg::empty_label);
        return;
    }
    scroll_ = settings_clamp_scroll(scroll_, images_.size(), grid_.h);
    std::optional<std::string> current = services_.wallpaper.own_image(selected_);
    cairo_save(cr);
    cairo_rectangle(cr, grid_.x, grid_.y, grid_.w, grid_.h);
    cairo_clip(cr);
    auto [first, last] = settings_grid_visible(grid_, images_.size(), scroll_);
    for (std::size_t i = first; i < last; ++i) {
        const std::string &path = images_[i];
        PanelRect cell = settings_grid_cell(grid_, i, scroll_);
        rounded_rect(cr, cell.x, cell.y, cell.w, cell.h, cfg::thumb_radius);
        if (cairo_surface_t *thumbnail = thumbnails_.get(path)) {
            cairo_save(cr);
            cairo_clip(cr);
            cairo_set_source_surface(cr, thumbnail, cell.x, cell.y);
            cairo_paint(cr);
            cairo_restore(cr);
        } else {
            set_source(cr, palette::text_alpha08);
            cairo_fill(cr);
            if (!thumbnails_.contains(path)) {
                wanted.push_back(path);
            }
        }
        if (current && *current == path) {
            constexpr double inset = cfg::thumb_border / 2.0;
            set_source(cr, palette::accent_alt);
            cairo_set_line_width(cr, cfg::thumb_border);
            rounded_rect(cr, cell.x + inset, cell.y + inset, cell.w - 2 * inset, cell.h - 2 * inset, cfg::thumb_radius - inset);
            cairo_stroke(cr);
        }
        hits.push_back({panel_intersect(cell, grid_), pick, path});
    }
    cairo_restore(cr);
    thumbnails_.request(wanted);
}

bool WallpaperTab::click(const std::optional<PanelHit> &hit) {
    bool was_editing = std::exchange(editing_, false);
    if (!hit) {
        return was_editing;
    }
    switch (hit->action) {
    case chip:
        selected_ = hit->tag;
        return true;
    case pick:
        services_.wallpaper.set_image(selected_, hit->tag);
        return true;
    case folder_field:
        editing_ = true;
        buffer_ = services_.settings.wallpaper_dir();
        return true;
    case reset:
        services_.wallpaper.clear_image(selected_);
        return true;
    default:
        return was_editing;
    }
}

bool WallpaperTab::key(const KeyEvent &event) {
    if (!editing_) {
        return false;
    }
    switch (event.kind) {
    case KeyKind::text:
        buffer_ += event.text;
        break;
    case KeyKind::backspace:
        pop_character(buffer_);
        break;
    case KeyKind::enter:
        editing_ = false;
        services_.settings.set_wallpaper_dir(buffer_);
        refresh();
        break;
    case KeyKind::escape:
        editing_ = false;
        break;
    default:
        break;
    }
    return true;
}

bool WallpaperTab::scroll(int direction) {
    double next = settings_clamp_scroll(scroll_ + direction * cfg::scroll_step, images_.size(), grid_.h);
    if (next == scroll_) {
        return false;
    }
    scroll_ = next;
    return true;
}

} // namespace astralia
