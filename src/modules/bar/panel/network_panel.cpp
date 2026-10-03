#include <algorithm>
#include <cstring>
#include <numbers>
#include <string>
#include <utility>

#include "config/bar_config.h"

#include "core/log.h"

#include "modules/bar/panel/network_panel.h"

#include "render/icons.h"

namespace astralia {

namespace {

struct Row {
    enum Kind { error,
                message,
                section,
                network } kind;
    int height;
    std::string text;
    const NetworkInfo *info = nullptr;
};

int signal_band(int percent) {
    return percent > 75 ? 3 : percent > 50 ? 2
                          : percent > 25   ? 1
                                           : 0;
}

bool secured(const NetworkInfo &info) {
    return !info.security.empty() && info.security != "--";
}

std::vector<Row> build_rows(const NetworkService &network) {
    std::vector<Row> rows;
    if (!network.last_error().empty()) {
        rows.push_back({Row::error, bar_config::panel_row_height, network.last_error()});
    }
    if (network.status().kind == NetworkKind::ethernet) {
        rows.push_back({Row::message, bar_config::panel_row_height, "Connected via Ethernet"});
    }
    if (!network.wifi_available()) {
        rows.push_back({Row::message, bar_config::panel_empty_height, "No Wi-Fi adapter found"});
        return rows;
    }
    if (!network.wifi_enabled()) {
        rows.push_back({Row::message, bar_config::panel_empty_height, "Wi-Fi is disabled"});
        return rows;
    }
    if (network_visible_count(network.networks()) == 0) {
        rows.push_back({Row::message, bar_config::panel_empty_height, "Scanning\xE2\x80\xA6"});
    }
    auto add_bucket = [&](const char *title, auto belongs) {
        std::vector<const NetworkInfo *> bucket;
        for (const auto &[ssid, info] : network.networks()) {
            if (belongs(info)) {
                bucket.push_back(&info);
            }
        }
        if (bucket.empty()) {
            return;
        }
        std::ranges::sort(bucket, [](const NetworkInfo *a, const NetworkInfo *b) {
            int band_a = signal_band(a->signal);
            int band_b = signal_band(b->signal);
            return band_a != band_b ? band_a > band_b : a->signal > b->signal;
        });
        rows.push_back({Row::section, bar_config::panel_section_height, title});
        for (const NetworkInfo *info : bucket) {
            rows.push_back({Row::network, bar_config::panel_row_height, info->ssid, info});
        }
    };
    add_bucket("Connected", [](const NetworkInfo &i) { return i.connected; });
    add_bucket("Known", [](const NetworkInfo &i) { return !i.connected && i.existing && i.in_range; });
    add_bucket("Available", [](const NetworkInfo &i) { return !i.connected && !i.existing; });
    return rows;
}

int rows_height(const std::vector<Row> &rows) {
    int height = 0;
    for (const Row &row : rows) {
        height += row.height + panel_config::row_gap;
    }
    return height;
}

SurfacePtr load_echo() {
    for (const char *dir : {ASTRALIA_POLKIT_DIR, ASTRALIA_SOURCE_POLKIT_DIR}) {
        auto echo = decode_image(std::string(dir) + "/" + bar_config::panel_echo_file, bar_config::panel_echo_size);
        if (echo) {
            return std::move(*echo);
        }
    }
    log::error("network panel: cannot load {}", bar_config::panel_echo_file);
    return nullptr;
}

std::size_t utf8_length(const std::string &text) {
    return static_cast<std::size_t>(std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

void draw_echo(cairo_t *cr, cairo_surface_t *echo, std::size_t count, double left, double cy, double max_width) {
    constexpr double size = bar_config::panel_echo_size;
    count = std::min(count, static_cast<std::size_t>(max_width / size));
    double x = left;
    for (std::size_t i = 0; i < count; ++i, x += size) {
        if (echo == nullptr) {
            set_source(cr, palette::electro);
            cairo_arc(cr, x + size / 2.0, cy, size / 3.0, 0.0, 2.0 * std::numbers::pi);
            cairo_fill(cr);
            continue;
        }
        cairo_save(cr);
        cairo_translate(cr, x, cy - size / 2.0);
        cairo_scale(cr, size / cairo_image_surface_get_width(echo), size / cairo_image_surface_get_height(echo));
        cairo_set_source_surface(cr, echo, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }
}

void pop_utf8(std::string &text) {
    while (!text.empty()) {
        unsigned char last = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((last & 0xC0) != 0x80) {
            break;
        }
    }
}

} // namespace

const char *network_signal_icon(int percent) {
    switch (signal_band(percent)) {
    case 3:
        return icon::wifi;
    case 2:
        return icon::wifi2;
    case 1:
        return icon::wifi1;
    default:
        return icon::wifi0;
    }
}

NetworkPanel::NetworkPanel(XConnection &x, EventLoop &loop, NetworkService &network)
    : network_(network),
      window_(x, loop, "astralia-network-panel", bar_config::panel_width, bar_config::panel_max_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] {
                  network_.stop_watch();
                  scroll_ = 0;
                  sub_ = Sub::none;
                  explicit_bzero(password_.data(), password_.size());
                  password_.clear(); }), echo_(load_echo()) {
    network_.changed.connect([this] {
        if (window_.is_open()) {
            paint();
        }
    });
}

void NetworkPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    network_.start_watch();
    paint();
    window_.open(bar_config::margin_x, bar_config::panel_top);
}

void NetworkPanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS:
        key(reinterpret_cast<const xcb_key_press_event_t &>(event));
        break;
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

void NetworkPanel::key(const xcb_key_press_event_t &event) {
    KeyEvent pressed = window_.keyboard().press(event.detail, event.state);
    if (pressed.kind == KeyKind::escape) {
        if (sub_ != Sub::none) {
            close_sub();
        } else {
            window_.close();
        }
        return;
    }
    if (sub_ != Sub::password) {
        return;
    }
    switch (pressed.kind) {
    case KeyKind::text:
        password_ += pressed.text;
        explicit_bzero(pressed.text.data(), pressed.text.size());
        break;
    case KeyKind::backspace:
        pop_utf8(password_);
        break;
    case KeyKind::enter:
        submit();
        return;
    default:
        return;
    }
    paint();
}

void NetworkPanel::scroll(int delta) {
    int next = panel_clamp_scroll(scroll_ + delta, content_height_, visible_height_);
    if (next != scroll_) {
        scroll_ = next;
        paint();
    }
}

void NetworkPanel::open_sub(Sub sub, const std::string &ssid) {
    explicit_bzero(password_.data(), password_.size());
    password_.clear();
    sub_ = sub;
    sub_ssid_ = ssid;
    paint();
}

void NetworkPanel::close_sub() {
    open_sub(Sub::none, {});
}

void NetworkPanel::submit() {
    switch (sub_) {
    case Sub::password:
        if (password_.size() < bar_config::panel_password_min) {
            return;
        }
        network_.connect(sub_ssid_, password_);
        break;
    case Sub::disconnect:
        network_.disconnect(sub_ssid_);
        break;
    case Sub::forget:
        network_.forget(sub_ssid_);
        break;
    case Sub::none:
        return;
    }
    close_sub();
}

void NetworkPanel::click(int x, int y) {
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    if (!hit) {
        if (y >= main_height_ && sub_ == Sub::none) {
            window_.close();
        }
        return;
    }
    switch (hit->action) {
    case close_panel:
        window_.close();
        return;
    case wifi:
        network_.set_wifi_enabled(!network_.wifi_enabled());
        return;
    case rescan:
        network_.scan();
        return;
    case dismiss_error:
        network_.clear_error();
        return;
    case network: {
        auto it = network_.networks().find(hit->tag);
        if (it == network_.networks().end() || network_.connecting_to() == hit->tag) {
            return;
        }
        const NetworkInfo &info = it->second;
        if (info.connected) {
            open_sub(Sub::disconnect, info.ssid);
        } else if (info.existing || !secured(info)) {
            network_.connect(info.ssid, {});
        } else {
            open_sub(Sub::password, info.ssid);
        }
        return;
    }
    case forget:
        open_sub(Sub::forget, hit->tag);
        return;
    case cancel:
        close_sub();
        return;
    case confirm:
        submit();
        return;
    default:
        return;
    }
}

void NetworkPanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    std::vector<Row> rows = build_rows(network_);
    double prompt_height = sub_ == Sub::password ? bar_config::panel_echo_row_height : panel_config::label_height;
    double sub_space = sub_ != Sub::none ? bar_config::panel_card_gap + panel_confirm_height(prompt_height) : 0.0;
    double top = panel_content_top();
    content_height_ = rows_height(rows);
    main_height_ = static_cast<int>(std::min<double>(window_.max_height() - sub_space, top + content_height_ + pad));
    visible_height_ = static_cast<int>(main_height_ - top - pad);
    scroll_ = panel_clamp_scroll(scroll_, content_height_, visible_height_);
    window_.set_height(static_cast<int>(main_height_ + sub_space));

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, main_height_);
    double controls = panel_draw_header(cr, width, "Network", hits_, close_panel);
    if (network_.wifi_available()) {
        double header_mid = pad + panel_config::header_height / 2.0;
        PanelRect toggle = panel_draw_toggle(cr, controls - panel_config::toggle_width,
                                             header_mid - panel_config::toggle_height / 2.0, network_.wifi_enabled());
        hits_.push_back({toggle, wifi, {}});
        PanelRect refresh = panel_draw_icon_button(cr, toggle.x - panel_config::row_gap - panel_config::button_size,
                                                   header_mid - panel_config::button_size / 2.0, icon::refresh,
                                                   network_.scanning() ? palette::accent : palette::text);
        hits_.push_back({refresh, rescan, {}});
    }

    PanelRect area{0, top, width, static_cast<double>(visible_height_)};
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.w, area.h);
    cairo_clip(cr);
    double y = top - scroll_;
    for (const Row &row : rows) {
        PanelRect rect{pad, y, width - 2 * pad, static_cast<double>(row.height)};
        if (y + row.height > area.y && y < area.y + area.h) {
            switch (row.kind) {
            case Row::error: {
                double reserve = panel_config::button_size + 8.0;
                panel_draw_device_row(cr, rect, icon::alert_triangle, row.text, {}, palette::critical_alpha15, palette::critical, reserve);
                PanelRect button = panel_draw_icon_button(cr, rect.x + rect.w - reserve, rect.y + (rect.h - panel_config::button_size) / 2.0, icon::close, palette::critical);
                hits_.push_back({panel_intersect(button, area), dismiss_error, {}});
                break;
            }
            case Row::message:
                panel_draw_centered(cr, rect, row.text);
                break;
            case Row::section:
                panel_draw_section(cr, y, row.height, row.text);
                break;
            case Row::network: {
                const NetworkInfo &info = *row.info;
                bool busy = network_.connecting_to() == info.ssid;
                bool portal = info.connected && network_.status().portal;
                bool can_forget = info.existing && !info.connected && !busy;
                const char *action_icon = info.connected ? icon::network_disconnect
                                          : busy         ? nullptr
                                                         : icon::network_connect;
                std::string subtitle = busy            ? "Connecting\xE2\x80\xA6"
                                       : portal        ? "Sign in required"
                                       : secured(info) ? info.security
                                                       : "Open";
                const Color &background = info.connected ? palette::accent_alpha25
                                          : busy         ? palette::accent_alpha12
                                                         : palette::text_alpha06;
                const Color &foreground = portal           ? palette::warn
                                          : info.connected ? palette::accent
                                                           : palette::text;
                constexpr double slot = panel_config::button_size + 8.0;
                double reserve = slot * ((can_forget ? 1 : 0) + (action_icon != nullptr ? 1 : 0));
                panel_draw_device_row(cr, rect, network_signal_icon(info.signal), info.ssid, subtitle, background, foreground, reserve);
                double button_x = rect.x + rect.w - reserve;
                double button_y = rect.y + (rect.h - panel_config::button_size) / 2.0;
                if (can_forget) {
                    PanelRect button = panel_draw_icon_button(cr, button_x, button_y, icon::close, palette::text_muted);
                    hits_.push_back({panel_intersect(button, area), forget, info.ssid});
                    button_x += slot;
                }
                if (action_icon != nullptr) {
                    panel_draw_icon_button(cr, button_x, button_y, action_icon, foreground);
                }
                hits_.push_back({panel_intersect(rect, area), network, info.ssid});
                break;
            }
            }
        }
        y += row.height + panel_config::row_gap;
    }
    cairo_restore(cr);

    double sub_y = main_height_ + bar_config::panel_card_gap;
    switch (sub_) {
    case Sub::password: {
        panel_draw_confirm(cr, sub_y, width, sub_ssid_, password_.empty() ? "Type the password" : "", "Connect", hits_, cancel, confirm, prompt_height);
        double echo_cy = sub_y + pad + panel_config::label_height + prompt_height / 2.0;
        draw_echo(cr, echo_.get(), utf8_length(password_), pad, echo_cy, width - 2 * pad);
        break;
    }
    case Sub::disconnect:
        panel_draw_confirm(cr, sub_y, width, sub_ssid_, "Disconnect from this network?", "Disconnect", hits_, cancel, confirm);
        break;
    case Sub::forget:
        panel_draw_confirm(cr, sub_y, width, sub_ssid_, "Forget this network?", "Forget", hits_, cancel, confirm);
        break;
    case Sub::none:
        break;
    }
    window_.present();
}

} // namespace astralia
