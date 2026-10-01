#include <algorithm>
#include <array>
#include <cairo-xcb.h>
#include <cstdlib>
#include <cstring>
#include <numbers>
#include <string_view>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config/polkit_config.h"

#include "core/log.h"

#include "modules/polkit.h"
#include "modules/polkit/layout.h"

namespace astralia {

namespace {

namespace cfg = polkit_config;

void set_source(cairo_t *cr, const Color &color) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}

void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    r = std::min({r, w / 2.0, h / 2.0});
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -std::numbers::pi / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, std::numbers::pi / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, std::numbers::pi / 2.0, std::numbers::pi);
    cairo_arc(cr, x + r, y + r, r, std::numbers::pi, 3.0 * std::numbers::pi / 2.0);
    cairo_close_path(cr);
}

void panel(cairo_t *cr, double x, double y, double w, double h, double r, const Color &fill) {
    rounded_rect(cr, x, y, w, h, r);
    set_source(cr, fill);
    cairo_fill(cr);
    double inset = cfg::border_width / 2.0;
    rounded_rect(cr, x + inset, y + inset, w - cfg::border_width, h - cfg::border_width, r - inset);
    set_source(cr, cfg::border);
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

Polkit::Polkit(XConnection &x, EventLoop &loop)
    : x_(x), keyboard_(x.conn()), title_(cfg::title_font), message_(cfg::message_font),
      field_(cfg::field_font), info_(cfg::info_font), echo_(load_echo()),
      service_(loop, [this] { sync(); }) {
    xcb_connection_t *conn = x_.conn();
    visual_ = x_.argb_visual();
    depth_ = 32;
    if (visual_ == nullptr) {
        visual_ = x_.visual();
        depth_ = x_.screen()->root_depth;
    }
    colormap_ = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap_, x_.root(), visual_->visual_id);
    window_ = xcb_generate_id(conn);
    std::array<uint32_t, 5> values{XCB_BACK_PIXMAP_NONE, 0, 1,
                                   XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                                       XCB_EVENT_MASK_KEY_PRESS,
                                   colormap_};
    OutputGeometry output = x_.primary_output();
    xcb_create_window(conn, depth_, window_, x_.root(), output.x, output.y, output.width,
                      output.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      values.data());
    using namespace std::string_view_literals;
    constexpr std::string_view name = "astralia-polkit"sv;
    constexpr std::string_view wm_class = "astralia-polkit\0astralia-shell\0"sv;
    xcb_ewmh_set_wm_name(x_.ewmh(), window_, name.size(), name.data());
    xcb_icccm_set_wm_class(conn, window_, wm_class.size(), wm_class.data());
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, window_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);
    loop.on_window(window_, [this](const xcb_generic_event_t &event) { handle(event); });
}

Polkit::~Polkit() {
    clear_password();
    xcb_connection_t *conn = x_.conn();
    if (open_) {
        restore_focus();
    }
    if (cr_ != nullptr) {
        cairo_destroy(cr_);
        cairo_surface_destroy(surface_);
        xcb_free_pixmap(conn, pixmap_);
    }
    xcb_free_gc(conn, gc_);
    xcb_destroy_window(conn, window_);
    xcb_free_colormap(conn, colormap_);
    xcb_flush(conn);
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
    place(pointer_output());
    keyboard_.reload();
    open_ = true;
    error_shown_ = false;
    last_error_ = false;
    paint();
    xcb_connection_t *conn = x_.conn();
    uint32_t above = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, window_, XCB_CONFIG_WINDOW_STACK_MODE, &above);
    xcb_map_window(conn, window_);
    take_focus();
    xcb_flush(conn);
    log::info("polkit: open");
}

void Polkit::close() {
    clear_password();
    xcb_connection_t *conn = x_.conn();
    restore_focus();
    xcb_unmap_window(conn, window_);
    xcb_flush(conn);
    open_ = false;
    error_shown_ = false;
    last_error_ = false;
}

void Polkit::place(const OutputGeometry &output) {
    xcb_connection_t *conn = x_.conn();
    if (output.width != geometry_.width || output.height != geometry_.height || cr_ == nullptr) {
        if (cr_ != nullptr) {
            cairo_destroy(cr_);
            cairo_surface_destroy(surface_);
            xcb_free_pixmap(conn, pixmap_);
        }
        pixmap_ = xcb_generate_id(conn);
        xcb_create_pixmap(conn, depth_, pixmap_, window_, output.width, output.height);
        surface_ = cairo_xcb_surface_create(conn, pixmap_, visual_, output.width, output.height);
        cr_ = cairo_create(surface_);
        scale_ = ui_scale(output);
        cairo_scale(cr_, scale_, scale_);
    }
    if (output != geometry_) {
        std::array<uint32_t, 4> values{static_cast<uint32_t>(output.x),
                                       static_cast<uint32_t>(output.y), output.width,
                                       output.height};
        xcb_configure_window(conn, window_,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             values.data());
    }
    geometry_ = output;
}

OutputGeometry Polkit::pointer_output() const {
    xcb_query_pointer_reply_t *reply =
        xcb_query_pointer_reply(x_.conn(), xcb_query_pointer(x_.conn(), x_.root()), nullptr);
    OutputGeometry fallback = x_.primary_output();
    if (reply == nullptr) {
        return fallback;
    }
    int px = reply->root_x;
    int py = reply->root_y;
    free(reply);
    for (const Output &output : x_.outputs()) {
        const OutputGeometry &g = output.geometry;
        if (px >= g.x && px < g.x + g.width && py >= g.y && py < g.y + g.height) {
            return g;
        }
    }
    return fallback;
}

void Polkit::take_focus() {
    xcb_connection_t *conn = x_.conn();
    xcb_get_input_focus_reply_t *reply =
        xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    xcb_window_t focus = reply != nullptr ? reply->focus : XCB_NONE;
    free(reply);
    if (focus != window_) {
        previous_focus_ = focus;
    }
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, window_, XCB_CURRENT_TIME);
}

void Polkit::restore_focus() {
    xcb_connection_t *conn = x_.conn();
    xcb_get_input_focus_reply_t *reply =
        xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    bool focused = reply != nullptr && reply->focus == window_;
    free(reply);
    if (focused) {
        xcb_window_t target = previous_focus_ != XCB_NONE && previous_focus_ != window_
                                  ? previous_focus_
                                  : static_cast<xcb_window_t>(XCB_INPUT_FOCUS_POINTER_ROOT);
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, target, XCB_CURRENT_TIME);
    }
    previous_focus_ = XCB_NONE;
}

void Polkit::handle(const xcb_generic_event_t &event) {
    if (!open_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        present();
        break;
    case XCB_KEY_PRESS: {
        const auto &press = reinterpret_cast<const xcb_key_press_event_t &>(event);
        key(keyboard_.press(press.detail, press.state));
        break;
    }
    case XCB_BUTTON_PRESS:
        take_focus();
        xcb_flush(x_.conn());
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
    double card_x = (geometry_.width / scale_ - cfg::card_width) / 2.0;
    double card_y = (geometry_.height / scale_ - card_h) / 2.0;

    cairo_t *cr = cr_;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    panel(cr, card_x, card_y, cfg::card_width, card_h, cfg::card_radius, cfg::background);

    double content_x = card_x + cfg::card_pad;
    double content_w = cfg::card_width - cfg::card_pad * 2.0;
    double content_cx = content_x + content_w / 2.0;
    double y = card_y + cfg::card_pad;

    set_source(cr, cfg::title);
    title_.set(cfg::title_text);
    title_.draw(cr, content_x, y + (cfg::title_line_height - title_.height()) / 2.0);
    y += cfg::title_line_height + cfg::spacing;

    set_source(cr, cfg::muted);
    std::string message = service_.message();
    message_.set(message);
    while (message_.width() > content_w && !message.empty()) {
        pop_utf8(message);
        message_.set(message + "…");
    }
    message_.draw(cr, content_x, y + (cfg::message_line_height - message_.height()) / 2.0);
    y += cfg::message_line_height + cfg::spacing;

    panel(cr, content_x, y, content_w, cfg::field_height, cfg::field_radius, cfg::field_background);
    double field_cy = y + cfg::field_height / 2.0;
    double dots_w = content_w - cfg::dot_margin * 2.0;
    std::size_t dots = polkit_visible_dots(utf8_length(password_), dots_w);
    if (needs_input && error_shown_) {
        set_source(cr, cfg::error);
        field_.set(cfg::error_text);
        field_.draw_ink_centered(cr, content_cx, field_cy);
    } else if (needs_input && dots == 0) {
        set_source(cr, cfg::muted);
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
                set_source(cr, cfg::dot);
                cairo_arc(cr, gx + cfg::dot_size / 2.0, field_cy, cfg::dot_size / 2.0, 0.0,
                          2.0 * std::numbers::pi);
                cairo_fill(cr);
            }
        }
    } else {
        set_source(cr, cfg::muted);
        field_.set(cfg::authenticating_text);
        field_.draw_ink_centered(cr, content_cx, field_cy);
    }
    y += cfg::field_height;

    if (show_info) {
        y += cfg::spacing;
        set_source(cr, cfg::muted);
        info_.set(info);
        info_.draw(cr, content_x, y + (cfg::info_line_height - info_.height()) / 2.0);
    }
    present();
}

void Polkit::present() {
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, 0, 0, 0, 0, geometry_.width, geometry_.height);
    xcb_flush(x_.conn());
}

} // namespace astralia
