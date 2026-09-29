#include <algorithm>
#include <array>
#include <cairo-xcb.h>
#include <cstdlib>
#include <malloc.h>
#include <numbers>
#include <string>
#include <string_view>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config/logout_config.h"

#include "core/log.h"
#include "core/spawn.h"

#include "modules/logout.h"
#include "modules/logout/layout.h"

namespace astralia {

namespace {

namespace cfg = logout_config;

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

SurfacePtr load_logo() {
    for (const char *dir : {ASTRALIA_LOGOUT_DIR, ASTRALIA_SOURCE_LOGOUT_DIR}) {
        auto logo = decode_image(std::string(dir) + "/" + cfg::logo_file, cfg::logo_size);
        if (logo) {
            return std::move(*logo);
        }
    }
    log::error("logout: cannot load {}", cfg::logo_file);
    return nullptr;
}

} // namespace

Logout::Logout(XConnection &x, EventLoop &loop, IpcServer &ipc)
    : x_(x), keyboard_(x.conn()), glyph_(cfg::glyph_font) {
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
                                       XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_LEAVE_WINDOW |
                                       XCB_EVENT_MASK_KEY_PRESS |
                                       XCB_EVENT_MASK_FOCUS_CHANGE,
                                   colormap_};
    OutputGeometry output = x_.primary_output();
    xcb_create_window(conn, depth_, window_, x_.root(), output.x, output.y, output.width,
                      output.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      values.data());
    using namespace std::string_view_literals;
    constexpr std::string_view name = "astralia-logout"sv;
    constexpr std::string_view wm_class = "astralia-logout\0astralia-shell\0"sv;
    xcb_ewmh_set_wm_name(x_.ewmh(), window_, name.size(), name.data());
    xcb_icccm_set_wm_class(conn, window_, wm_class.size(), wm_class.data());
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, window_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);

    loop.on_window(window_, [this](const xcb_generic_event_t &event) { handle(event); });
    ipc.add({"logout",
             [this] {
                 toggle();
                 return std::string();
             },
             "toggle the logout overlay"});
}

Logout::~Logout() {
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

void Logout::toggle() {
    if (open_) {
        close();
    } else {
        open();
    }
}

void Logout::open() {
    place(pointer_output());
    keyboard_.reload();
    logo_ = load_logo();
    selected_ = 0;
    hovered_ = -1;
    open_ = true;
    paint();
    xcb_connection_t *conn = x_.conn();
    uint32_t above = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, window_, XCB_CONFIG_WINDOW_STACK_MODE, &above);
    xcb_map_window(conn, window_);
    take_focus();
    log::info("logout: open");
}

void Logout::close() {
    xcb_connection_t *conn = x_.conn();
    restore_focus();
    xcb_unmap_window(conn, window_);
    xcb_flush(conn);
    open_ = false;
    hovered_ = -1;
    logo_.reset();
    if (cr_ != nullptr) {
        cairo_destroy(cr_);
        cairo_surface_destroy(surface_);
        xcb_free_pixmap(conn, pixmap_);
        cr_ = nullptr;
        surface_ = nullptr;
        pixmap_ = XCB_NONE;
    }
    malloc_trim(0);
}

void Logout::place(const OutputGeometry &output) {
    xcb_connection_t *conn = x_.conn();
    pixmap_ = xcb_generate_id(conn);
    xcb_create_pixmap(conn, depth_, pixmap_, window_, output.width, output.height);
    surface_ = cairo_xcb_surface_create(conn, pixmap_, visual_, output.width, output.height);
    cr_ = cairo_create(surface_);
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

OutputGeometry Logout::pointer_output() const {
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

void Logout::take_focus() {
    xcb_connection_t *conn = x_.conn();
    xcb_get_input_focus_reply_t *reply =
        xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr);
    previous_focus_ = reply != nullptr ? reply->focus : XCB_NONE;
    free(reply);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, window_, XCB_CURRENT_TIME);
}

void Logout::restore_focus() {
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

void Logout::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        if (open_) {
            present();
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
        hover(button_at(motion.event_x, motion.event_y));
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

void Logout::key(const KeyEvent &event) {
    switch (event.kind) {
    case KeyKind::left:
        selected_ = (selected_ + cfg::button_count - 1) % cfg::button_count;
        break;
    case KeyKind::right:
        selected_ = (selected_ + 1) % cfg::button_count;
        break;
    case KeyKind::text:
        if (event.text.size() != 1 || event.text[0] < '1' ||
            event.text[0] >= '1' + cfg::button_count) {
            return;
        }
        selected_ = event.text[0] - '1';
        break;
    case KeyKind::enter:
        execute(selected_);
        return;
    case KeyKind::escape:
        close();
        return;
    default:
        return;
    }
    paint();
}

void Logout::click(double x, double y) {
    if (std::optional<int> button = button_at(x, y)) {
        execute(*button);
    } else {
        close();
    }
}

void Logout::hover(std::optional<int> button) {
    int next = open_ && button ? *button : -1;
    if (next != hovered_) {
        hovered_ = next;
        if (open_) {
            paint();
        }
    }
}

void Logout::execute(int index) {
    const char *command = cfg::actions[index].command;
    close();
    if (command[0] != '\0') {
        spawn_detached(command);
    }
}

std::optional<int> Logout::button_at(double x, double y) const {
    return logout_button_at({x, y}, {geometry_.width / 2.0, geometry_.height / 2.0});
}

void Logout::paint() {
    cairo_t *cr = cr_;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    Point center{geometry_.width / 2.0, geometry_.height / 2.0};
    for (int i = 0; i < cfg::button_count; ++i) {
        bool highlighted = i == selected_ || i == hovered_;
        double scale = highlighted ? cfg::highlight_scale : 1.0;
        double size = cfg::button_size * scale;
        Point c = logout_button_center(i, center);
        double x = c.x - size / 2.0;
        double y = c.y - size / 2.0;
        double radius = cfg::button_corner_radius * scale;
        rounded_rect(cr, x, y, size, size, radius);
        set_source(cr, cfg::button_fill);
        cairo_fill(cr);
        constexpr double inset = cfg::border_width / 2.0;
        rounded_rect(cr, x + inset, y + inset, size - cfg::border_width, size - cfg::border_width,
                     radius - inset);
        set_source(cr, highlighted ? cfg::highlight_border : cfg::border);
        cairo_set_line_width(cr, cfg::border_width);
        cairo_stroke(cr);

        glyph_.set(cfg::actions[i].glyph);
        set_source(cr, cfg::glyph);
        glyph_.draw_ink_centered(cr, c.x, c.y);
    }

    if (logo_) {
        int w = cairo_image_surface_get_width(logo_.get());
        int h = cairo_image_surface_get_height(logo_.get());
        cairo_set_source_surface(cr, logo_.get(), center.x - w / 2.0, center.y - h / 2.0);
        cairo_paint(cr);
    }
    present();
}

void Logout::present() {
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, 0, 0, 0, 0, geometry_.width, geometry_.height);
    xcb_flush(x_.conn());
}

} // namespace astralia
