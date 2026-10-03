#include <algorithm>
#include <format>

#include "config/bar_config.h"
#include "config/panel_config.h"

#include "modules/bar/panel/battery_panel.h"

namespace astralia {

std::string battery_time_left(int seconds) {
    if (seconds <= 0) {
        return {};
    }
    int hours = seconds / 3600;
    int minutes = seconds % 3600 / 60;
    return hours > 0 ? std::format("{}h {}m", hours, minutes) : std::format("{}m", minutes);
}

std::string battery_state_label(const BatteryStatus &status) {
    std::string state = status.charging  ? "Charging"
                        : status.full    ? "Full"
                        : status.pending ? "Not charging"
                                         : "Discharging";
    std::string time = status.full || status.pending ? std::string() : battery_time_left(status.seconds_left);
    return time.empty() ? state : state + " \xE2\x80\x94 " + time + (status.charging ? " to full" : " left");
}

BatteryPanel::BatteryPanel(XConnection &x, EventLoop &loop, BatteryService &battery)
    : battery_(battery),
      window_(x, loop, "astralia-battery-panel", panel_config::width, panel_config::max_height, [this](const xcb_generic_event_t &event) { handle(event); }, nullptr) {
    battery_.changed.connect([this] {
        if (window_.is_open()) {
            paint();
        }
    });
}

void BatteryPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    paint();
    window_.open(bar_config::margin_x, bar_config::panel_top);
}

void BatteryPanel::handle(const xcb_generic_event_t &event) {
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
        if (button.detail == XCB_BUTTON_INDEX_1 && hit && hit->action == close_panel) {
            window_.close();
        }
        break;
    }
    default:
        break;
    }
}

void BatteryPanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    constexpr double label = panel_config::label_height;
    constexpr double bar_h = panel_config::track_height + 2;
    const BatteryStatus &status = battery_.status();
    double top = panel_content_top();
    double content = status.present ? label + panel_config::row_gap + bar_h : panel_config::empty_height;
    int height = static_cast<int>(top + content + pad);
    window_.set_height(height);

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, height);
    panel_draw_header(cr, width, "Battery", hits_, close_panel);
    double inner = width - 2 * pad;
    if (!status.present) {
        panel_draw_centered(cr, {pad, top, inner, content}, "No battery detected");
        window_.present();
        return;
    }

    const Color &fill = status.charging || status.full ? palette::accent
                        : status.percent <= 15         ? palette::critical
                        : status.percent <= 30         ? palette::warn
                                                       : palette::text_muted;
    int title_w = panel_draw_text(cr, panel_config::font, "Battery", pad, top, label, 0, bar_config::foreground);
    double state_x = pad + title_w + panel_config::row_gap;
    panel_draw_text(cr, panel_config::small_font, battery_state_label(status), state_x, top, label,
                    static_cast<int>(pad + inner - state_x), status.charging ? palette::accent : palette::text_dim);

    std::string percent = std::format("{}%", status.percent);
    int percent_w = panel_text_width(panel_config::small_font, percent);
    double bar_y = top + label + panel_config::row_gap;
    double bar_w = inner - percent_w - panel_config::row_gap;
    set_source(cr, palette::text_alpha08);
    rounded_rect(cr, pad, bar_y, bar_w, bar_h, bar_h / 2.0);
    cairo_fill(cr);
    set_source(cr, fill);
    rounded_rect(cr, pad, bar_y, bar_w * std::clamp(status.percent, 0, 100) / 100.0, bar_h, bar_h / 2.0);
    cairo_fill(cr);
    panel_draw_text(cr, panel_config::small_font, percent, pad + inner - percent_w, bar_y - label / 2.0 + bar_h / 2.0, label, 0, fill);
    window_.present();
}

} // namespace astralia
