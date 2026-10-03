#include <algorithm>
#include <cstring>
#include <numbers>

#include "config/polkit_config.h"

#include "core/log.h"

#include "modules/polkit.h"
#include "modules/polkit/layout.h"

#include "render/draw.h"

namespace astralia {

namespace {

namespace cfg = polkit_config;

void panel(cairo_t *cr, double x, double y, double w, double h, double r, const Color &fill) {
    rounded_rect(cr, x, y, w, h, r);
    set_source(cr, fill);
    cairo_fill(cr);
    double inset = cfg::border_width / 2.0;
    rounded_rect(cr, x + inset, y + inset, w - cfg::border_width, h - cfg::border_width, r - inset);
    set_source(cr, palette::accent);
    cairo_set_line_width(cr, cfg::border_width);
    cairo_stroke(cr);
}

void pop_utf8(std::string &text) {
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

SurfacePtr load_echo() {
    for (const char *dir : {ASTRALIA_POLKIT_DIR, ASTRALIA_SOURCE_POLKIT_DIR}) {
        auto echo = decode_image(std::string(dir) + "/" + cfg::echo_file, cfg::dot_size);
        if (echo) {
            return std::move(*echo);
        }
    }
    log::error("polkit: cannot load {}", cfg::echo_file);
    return nullptr;
}

} // namespace

Polkit::Polkit(XConnection &x, EventLoop &loop, Services &services)
    : x_(x), window_(x, "astralia-polkit", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_PRESS),
      keyboard_(x.conn()), title_(cfg::title_font), message_(cfg::message_font),
      field_(cfg::field_font), info_(cfg::info_font), echo_(load_echo()),
      service_(services.polkit) {
    service_.changed.connect([this] { sync(); });
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
}

Polkit::~Polkit() {
    clear_password();
}

void Polkit::sync() {
    bool pending = service_.pending();
    if (pending && !open_) {
        open();
        return;
    }
    if (!pending && open_) {
        close();
        return;
    }
    if (open_) {
        paint();
    }
}

void Polkit::open() {
    window_.place(x_.pointer_output());
    keyboard_.reload();
    open_ = true;
    error_shown_ = false;
    last_error_ = false;
    paint();
    window_.show(true);
    log::info("polkit: open");
}

void Polkit::close() {
    clear_password();
    window_.hide();
    open_ = false;
    error_shown_ = false;
    last_error_ = false;
}

void Polkit::handle(const xcb_generic_event_t &event) {
    if (!open_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        window_.present();
        break;
    case XCB_KEY_PRESS: {
        const auto &press = reinterpret_cast<const xcb_key_press_event_t &>(event);
        key(keyboard_.press(press.detail, press.state));
        break;
    }
    case XCB_BUTTON_PRESS:
        window_.focus();
        break;
    default:
        break;
    }
}

void Polkit::key(const KeyEvent &event) {
    switch (event.kind) {
    case KeyKind::text:
        password_ += event.text;
        error_shown_ = false;
        break;
    case KeyKind::backspace:
        pop_utf8(password_);
        error_shown_ = false;
        break;
    case KeyKind::enter:
        if (password_.empty()) {
            return;
        }
        error_shown_ = false;
        service_.respond(password_);
        return;
    case KeyKind::escape:
        service_.cancel();
        return;
    case KeyKind::up:
    case KeyKind::down:
    case KeyKind::left:
    case KeyKind::right:
    case KeyKind::none:
        return;
    }
    paint();
}

void Polkit::clear_password() {
    explicit_bzero(password_.data(), password_.size());
    password_.clear();
}

void Polkit::paint() {
    bool needs_input = service_.response_required();
    std::string info = service_.info();
    bool info_error = service_.info_is_error();
    if (info_error && !last_error_) {
        error_shown_ = true;
    }
    last_error_ = info_error;
    bool show_info = !info.empty() && !info_error;

    double card_h = polkit_card_height(show_info);
    double card_x = (window_.geometry().width - cfg::card_width) / 2.0;
    double card_y = (window_.geometry().height - card_h) / 2.0;

    cairo_t *cr = window_.cr();
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    panel(cr, card_x, card_y, cfg::card_width, card_h, cfg::card_radius, palette::overlay);

    double content_x = card_x + cfg::card_pad;
    double content_w = cfg::card_width - cfg::card_pad * 2.0;
    double content_cx = content_x + content_w / 2.0;
    double y = card_y + cfg::card_pad;

    set_source(cr, palette::text);
    title_.set(cfg::title_text);
    title_.draw(cr, content_x, y + (cfg::title_line_height - title_.height()) / 2.0);
    y += cfg::title_line_height + cfg::spacing;

    set_source(cr, palette::text_muted);
    std::string message = service_.message();
    message_.set(message);
    while (message_.width() > content_w && !message.empty()) {
        pop_utf8(message);
        message_.set(message + "…");
    }
    message_.draw(cr, content_x, y + (cfg::message_line_height - message_.height()) / 2.0);
    y += cfg::message_line_height + cfg::spacing;

    panel(cr, content_x, y, content_w, cfg::field_height, cfg::field_radius, palette::field_bg);
    double field_cy = y + cfg::field_height / 2.0;
    double dots_w = content_w - cfg::dot_margin * 2.0;
    std::size_t dots = polkit_visible_dots(utf8_length(password_), dots_w);
    if (needs_input && error_shown_) {
        set_source(cr, palette::critical);
        field_.set(cfg::error_text);
        field_.draw_ink_centered(cr, content_cx, field_cy);
    } else if (needs_input && dots == 0) {
        set_source(cr, palette::text_muted);
        field_.set(cfg::password_placeholder);
        field_.draw_ink_centered(cr, content_cx, field_cy);
    } else if (needs_input) {
        double dot_x = content_cx - static_cast<double>(dots) * cfg::dot_size / 2.0;
        double dot_y = field_cy - cfg::dot_size / 2.0;
        for (std::size_t i = 0; i < dots; ++i) {
            double gx = dot_x + static_cast<double>(i) * cfg::dot_size;
            if (echo_ != nullptr) {
                double scale_x = static_cast<double>(cfg::dot_size) / cairo_image_surface_get_width(echo_.get());
                double scale_y = static_cast<double>(cfg::dot_size) / cairo_image_surface_get_height(echo_.get());
                cairo_save(cr);
                cairo_translate(cr, gx, dot_y);
                cairo_scale(cr, scale_x, scale_y);
                cairo_set_source_surface(cr, echo_.get(), 0, 0);
                cairo_paint(cr);
                cairo_restore(cr);
            } else {
                set_source(cr, palette::text);
                cairo_arc(cr, gx + cfg::dot_size / 2.0, field_cy, cfg::dot_size / 2.0, 0.0,
                          2.0 * std::numbers::pi);
                cairo_fill(cr);
            }
        }
    } else {
        set_source(cr, palette::text_muted);
        field_.set(cfg::authenticating_text);
        field_.draw_ink_centered(cr, content_cx, field_cy);
    }
    y += cfg::field_height;

    if (show_info) {
        y += cfg::spacing;
        set_source(cr, palette::text_muted);
        info_.set(info);
        info_.draw(cr, content_x, y + (cfg::info_line_height - info_.height()) / 2.0);
    }
    window_.present();
}

} // namespace astralia
