#include <algorithm>
#include <chrono>
#include <ctime>
#include <numbers>
#include <string>

#include "config/bar_config.h"

#include "modules/bar/panel/clock_panel.h"

#include "render/icons.h"
#include "render/text.h"

namespace astralia {

namespace {

constexpr std::array<const char *, 12> month_names = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr std::array<const char *, 7> weekday_names = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};

constexpr double left_width = (bar_config::clock_panel_width - 2.0 * panel_config::padding - bar_config::clock_column_gap) / 2.0;
constexpr double cell = (bar_config::clock_panel_width - 2.0 * panel_config::padding - left_width - bar_config::clock_column_gap) / 7.0;
constexpr double left_height = bar_config::clock_weekday_line + bar_config::clock_line_gap + bar_config::clock_date_line + bar_config::clock_line_gap + bar_config::clock_date_line + bar_config::clock_big_day_gap + bar_config::clock_big_day_row + bar_config::clock_big_day_gap + bar_config::clock_week_line;
constexpr double grid_height = bar_config::clock_grid_header + bar_config::clock_grid_header_gap + bar_config::clock_weekday_row + bar_config::clock_grid_top_gap + 6.0 * cell;
constexpr double content_height = std::max(left_height, grid_height);
constexpr int panel_height = static_cast<int>(2 * panel_config::padding + content_height + 1);

std::chrono::year_month_day to_ymd(int year, int month, int day) {
    return std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month + 1)} / std::chrono::day{static_cast<unsigned>(day)};
}

void draw_centered(cairo_t *cr, const char *font, const std::string &text, double x, double width, double top, double height, const Color &color) {
    panel_draw_text(cr, font, text, x + (width - panel_text_width(font, text)) / 2.0, top, height, 0, color);
}

void draw_circle(cairo_t *cr, double cx, double cy, double radius, const Color &color) {
    set_source(cr, color);
    cairo_arc(cr, cx, cy, radius, 0.0, 2.0 * std::numbers::pi);
    cairo_fill(cr);
}

PanelRect draw_nav_button(cairo_t *cr, double x, double y, const char *icon) {
    constexpr double size = bar_config::clock_nav_button;
    draw_circle(cr, x + size / 2.0, y + size / 2.0, size / 2.0, palette::text_alpha08);
    if (icon == nullptr) {
        draw_circle(cr, x + size / 2.0, y + size / 2.0, bar_config::clock_today_dot / 2.0, palette::accent);
    } else {
        Text glyph(panel_config::icon_font);
        glyph.set(icon);
        set_source(cr, palette::text);
        glyph.draw_ink_centered(cr, x + size / 2.0, y + size / 2.0);
    }
    return {x, y, size, size};
}

} // namespace

std::array<CalendarDay, 42> clock_panel_cells(int year, int month) {
    std::chrono::sys_days first{to_ymd(year, month, 1)};
    unsigned offset = std::chrono::weekday{first}.iso_encoding() - 1;
    std::chrono::sys_days start = first - std::chrono::days{offset};
    std::array<CalendarDay, 42> cells{};
    for (int i = 0; i < 42; ++i) {
        std::chrono::year_month_day ymd{start + std::chrono::days{i}};
        int cell_month = static_cast<int>(static_cast<unsigned>(ymd.month())) - 1;
        cells[static_cast<std::size_t>(i)] = {static_cast<int>(ymd.year()), cell_month, static_cast<int>(static_cast<unsigned>(ymd.day())), cell_month == month};
    }
    return cells;
}

CalendarMonth clock_panel_month_shifted(int year, int month, int delta) {
    std::chrono::year_month shifted = std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month + 1)} + std::chrono::months{delta};
    return {static_cast<int>(shifted.year()), static_cast<int>(static_cast<unsigned>(shifted.month())) - 1};
}

bool clock_panel_same_day(const CalendarDay &cell, int year, int month, int day) {
    return cell.year == year && cell.month == month && cell.day == day;
}

int clock_panel_iso_week(int year, int month, int day) {
    std::chrono::sys_days date{to_ymd(year, month, day)};
    std::chrono::sys_days thursday = date + std::chrono::days{4 - static_cast<int>(std::chrono::weekday{date}.iso_encoding())};
    std::chrono::sys_days jan1{std::chrono::year_month_day{thursday}.year() / std::chrono::January / 1};
    return static_cast<int>((thursday - jan1).count() / 7) + 1;
}

ClockPanel::ClockPanel(XConnection &x, EventLoop &loop)
    : x_(x), window_(x, loop, "astralia-clock-panel", bar_config::clock_panel_width, panel_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] { month_offset_ = 0; }) {}

void ClockPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    paint();
    window_.open((x_.primary_output().width - window_.width()) / 2, bar_config::panel_top);
}

void ClockPanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        if (window_.keyboard().press(key.detail, key.state).kind == KeyKind::escape) {
            window_.close();
        }
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        std::optional<PanelHit> hit = panel_hit_at(hits_, button.event_x, button.event_y);
        if (button.detail != XCB_BUTTON_INDEX_1 || !hit) {
            break;
        }
        month_offset_ = hit->action == previous ? month_offset_ - 1 : hit->action == next ? month_offset_ + 1
                                                                                          : 0;
        paint();
        break;
    }
    default:
        break;
    }
}

void ClockPanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    window_.set_height(panel_height);

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    int year = local.tm_year + 1900;
    int month = local.tm_mon;
    int day = local.tm_mday;
    CalendarMonth shown = clock_panel_month_shifted(year, month, month_offset_);

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, panel_height);

    char weekday[24]{};
    std::strftime(weekday, sizeof weekday, "%A", &local);
    char month_name[24]{};
    std::strftime(month_name, sizeof month_name, "%B", &local);
    double y = pad + (content_height - left_height) / 2.0;
    draw_centered(cr, panel_config::title_font, weekday, pad, left_width, y, bar_config::clock_weekday_line, palette::text);
    y += bar_config::clock_weekday_line + bar_config::clock_line_gap;
    draw_centered(cr, panel_config::font, month_name, pad, left_width, y, bar_config::clock_date_line, palette::text_muted);
    y += bar_config::clock_date_line + bar_config::clock_line_gap;
    draw_centered(cr, panel_config::font, std::to_string(year), pad, left_width, y, bar_config::clock_date_line, palette::text_muted);
    y += bar_config::clock_date_line + bar_config::clock_big_day_gap;
    draw_centered(cr, bar_config::clock_big_day_font, std::to_string(day), pad, left_width, y, bar_config::clock_big_day_row, palette::text);
    y += bar_config::clock_big_day_row + bar_config::clock_big_day_gap;
    draw_centered(cr, panel_config::font, "Week " + std::to_string(clock_panel_iso_week(year, month, day)), pad, left_width, y, bar_config::clock_week_line, palette::text_dim);

    double grid_x = pad + left_width + bar_config::clock_column_gap;
    double inset = (cell - panel_text_width(panel_config::font, weekday_names[0])) / 2.0;
    std::string title = std::string(month_names[static_cast<std::size_t>(shown.month)]) + " " + std::to_string(shown.year);
    panel_draw_text(cr, panel_config::font, title, grid_x + inset, pad, bar_config::clock_grid_header, 0, palette::text);

    constexpr double nav = bar_config::clock_nav_button;
    double nav_y = pad + (bar_config::clock_grid_header - nav) / 2.0;
    double nav_x = grid_x + 7 * cell - nav;
    hits_.push_back({draw_nav_button(cr, nav_x, nav_y, icon::chevron_right), next, {}});
    nav_x -= nav + bar_config::clock_nav_gap;
    hits_.push_back({draw_nav_button(cr, nav_x, nav_y, nullptr), today, {}});
    nav_x -= nav + bar_config::clock_nav_gap;
    hits_.push_back({draw_nav_button(cr, nav_x, nav_y, icon::chevron_left), previous, {}});

    double row_y = pad + bar_config::clock_grid_header + bar_config::clock_grid_header_gap;
    for (std::size_t col = 0; col < weekday_names.size(); ++col) {
        draw_centered(cr, panel_config::font, weekday_names[col], grid_x + col * cell, cell, row_y, bar_config::clock_weekday_row, palette::text);
    }

    double grid_y = row_y + bar_config::clock_weekday_row + bar_config::clock_grid_top_gap;
    std::array<CalendarDay, 42> cells = clock_panel_cells(shown.year, shown.month);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const CalendarDay &entry = cells[i];
        double cx = grid_x + static_cast<double>(i % 7) * cell;
        double cy = grid_y + static_cast<double>(i / 7) * cell;
        bool is_today = clock_panel_same_day(entry, year, month, day);
        if (is_today) {
            draw_circle(cr, cx + cell / 2.0, cy + cell / 2.0, cell / 2.0 - bar_config::clock_cell_padding, palette::accent);
        }
        const Color &color = is_today || entry.in_month ? palette::text : palette::text_dim;
        draw_centered(cr, panel_config::font, std::to_string(entry.day), cx, cell, cy, cell, color);
    }
    window_.present();
}

} // namespace astralia
