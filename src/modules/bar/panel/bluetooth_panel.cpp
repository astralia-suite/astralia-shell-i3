#include <algorithm>
#include <string>
#include <utility>

#include "config/bar_config.h"

#include "modules/bar/panel/bluetooth_panel.h"

#include "render/icons.h"

namespace astralia {

namespace {

struct Row {
    enum Kind { message,
                section,
                item } kind;
    int height;
    std::string text;
    const BluetoothDevice *device = nullptr;
};

std::vector<Row> build_rows(const BluetoothService &bluetooth) {
    const BluetoothStatus &status = bluetooth.status();
    if (!status.present) {
        return {{Row::message, bar_config::panel_empty_height, "No adapter found"}};
    }
    if (!status.powered) {
        return {{Row::message, bar_config::panel_empty_height, "Bluetooth is off"}};
    }
    std::vector<Row> rows;
    auto add_bucket = [&](const char *title, auto belongs) {
        bool first = true;
        for (const BluetoothDevice &d : bluetooth.devices()) {
            if (!belongs(d)) {
                continue;
            }
            if (std::exchange(first, false)) {
                rows.push_back({Row::section, bar_config::panel_section_height, title});
            }
            rows.push_back({Row::item, bar_config::panel_row_height, d.name, &d});
        }
    };
    add_bucket("Connected", [](const BluetoothDevice &d) { return d.connected; });
    add_bucket("Paired", [](const BluetoothDevice &d) { return !d.connected && (d.paired || d.trusted); });
    add_bucket("Nearby", [](const BluetoothDevice &d) { return !d.connected && !d.paired && !d.trusted; });
    if (rows.empty()) {
        rows.push_back({Row::message, bar_config::panel_empty_height, "Searching for devices\xE2\x80\xA6"});
    }
    return rows;
}

int rows_height(const std::vector<Row> &rows) {
    int height = 0;
    for (const Row &row : rows) {
        height += row.height + panel_config::row_gap;
    }
    return height;
}

} // namespace

BluetoothPanel::BluetoothPanel(XConnection &x, EventLoop &loop, BluetoothService &bluetooth)
    : bluetooth_(bluetooth),
      window_(x, loop, "astralia-bluetooth-panel", bar_config::panel_width, bar_config::panel_max_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] {
                  bluetooth_.stop_discovery();
                  scroll_ = 0;
                  confirm_path_.clear(); }) {
    bluetooth_.changed.connect([this] {
        if (window_.is_open()) {
            paint();
        }
    });
}

void BluetoothPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    bluetooth_.start_discovery();
    paint();
    window_.open(bar_config::margin_x, bar_config::panel_top);
}

void BluetoothPanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        if (window_.keyboard().press(key.detail, key.state).kind != KeyKind::escape) {
            break;
        }
        if (!confirm_path_.empty()) {
            close_confirm();
        } else {
            window_.close();
        }
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        if (button.detail == XCB_BUTTON_INDEX_1) {
            click(button.event_x, button.event_y);
        } else if (button.detail == XCB_BUTTON_INDEX_4) {
            scroll(-bar_config::panel_scroll_step);
        } else if (button.detail == XCB_BUTTON_INDEX_5) {
            scroll(bar_config::panel_scroll_step);
        }
        break;
    }
    default:
        break;
    }
}

void BluetoothPanel::scroll(int delta) {
    int next = panel_clamp_scroll(scroll_ + delta, content_height_, visible_height_);
    if (next != scroll_) {
        scroll_ = next;
        paint();
    }
}

void BluetoothPanel::close_confirm() {
    confirm_path_.clear();
    paint();
}

void BluetoothPanel::click(int x, int y) {
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    if (!hit) {
        if (y >= main_height_ && confirm_path_.empty()) {
            window_.close();
        }
        return;
    }
    switch (hit->action) {
    case close_panel:
        window_.close();
        return;
    case power:
        bluetooth_.set_powered(!bluetooth_.status().powered);
        return;
    case device: {
        auto it = std::ranges::find(bluetooth_.devices(), hit->tag, &BluetoothDevice::path);
        if (it == bluetooth_.devices().end() || it->connecting) {
            return;
        }
        if (it->connected) {
            confirm_action_ = device;
            confirm_path_ = it->path;
            paint();
        } else if (it->paired || it->trusted) {
            bluetooth_.connect(it->path);
        } else {
            bluetooth_.pair(it->path);
        }
        return;
    }
    case forget:
        confirm_action_ = forget;
        confirm_path_ = hit->tag;
        paint();
        return;
    case cancel:
        close_confirm();
        return;
    case confirm:
        if (confirm_action_ == device) {
            bluetooth_.disconnect(confirm_path_);
        } else {
            bluetooth_.forget(confirm_path_);
        }
        close_confirm();
        return;
    default:
        return;
    }
}

void BluetoothPanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    std::vector<Row> rows = build_rows(bluetooth_);
    const BluetoothDevice *target = nullptr;
    if (!confirm_path_.empty()) {
        auto it = std::ranges::find(bluetooth_.devices(), confirm_path_, &BluetoothDevice::path);
        if (it != bluetooth_.devices().end()) {
            target = &*it;
        } else {
            confirm_path_.clear();
        }
    }
    double confirm_space = target != nullptr ? bar_config::panel_card_gap + panel_confirm_height() : 0.0;
    double top = panel_content_top();
    content_height_ = rows_height(rows);
    main_height_ = static_cast<int>(std::min<double>(window_.max_height() - confirm_space, top + content_height_ + pad));
    visible_height_ = static_cast<int>(main_height_ - top - pad);
    scroll_ = panel_clamp_scroll(scroll_, content_height_, visible_height_);
    window_.set_height(static_cast<int>(main_height_ + confirm_space));

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, main_height_);
    double controls = panel_draw_header(cr, width, "Bluetooth", hits_, close_panel);
    if (bluetooth_.status().present) {
        PanelRect toggle = panel_draw_toggle(cr, controls - panel_config::toggle_width,
                                             pad + (panel_config::header_height - panel_config::toggle_height) / 2.0,
                                             bluetooth_.status().powered);
        hits_.push_back({toggle, power, {}});
    }

    PanelRect area{0, top, width, static_cast<double>(visible_height_)};
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.w, area.h);
    cairo_clip(cr);
    double y = top - scroll_;
    for (const Row &row : rows) {
        PanelRect rect{pad, y, width - 2 * pad, static_cast<double>(row.height)};
        if (y + row.height > area.y && y < area.y + area.h) {
            if (row.kind == Row::message) {
                panel_draw_centered(cr, rect, row.text);
            } else if (row.kind == Row::section) {
                panel_draw_section(cr, y, row.height, row.text);
            } else {
                const BluetoothDevice &d = *row.device;
                bool can_forget = (d.paired || d.trusted) && !d.connected && !d.connecting;
                std::string subtitle = d.connecting     ? "Connecting\xE2\x80\xA6"
                                       : d.battery >= 0 ? std::to_string(d.battery) + "%"
                                                        : std::string();
                const Color &background = d.connected    ? palette::accent_alpha25
                                          : d.connecting ? palette::accent_alpha12
                                                         : palette::text_alpha06;
                const Color &foreground = d.connected ? palette::accent : palette::text;
                double reserve = can_forget ? panel_config::button_size + 8.0 : 0.0;
                panel_draw_device_row(cr, rect, d.connected ? icon::bluetooth_connected : icon::bluetooth_device, d.name, subtitle, background, foreground, reserve);
                hits_.push_back({panel_intersect(rect, area), device, d.path});
                if (can_forget) {
                    PanelRect button = panel_draw_icon_button(cr, rect.x + rect.w - panel_config::button_size - 8.0,
                                                              rect.y + (rect.h - panel_config::button_size) / 2.0, icon::close, palette::text_muted);
                    hits_.insert(hits_.end() - 1, {panel_intersect(button, area), forget, d.path});
                }
            }
        }
        y += row.height + panel_config::row_gap;
    }
    cairo_restore(cr);

    if (target != nullptr) {
        bool disconnecting = confirm_action_ == device;
        panel_draw_confirm(cr, main_height_ + bar_config::panel_card_gap, width, target->name,
                           disconnecting ? "Disconnect this device?" : "Forget this device?",
                           disconnecting ? "Disconnect" : "Forget", hits_, cancel, confirm);
    }
    window_.present();
}

} // namespace astralia
