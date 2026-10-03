#include <algorithm>
#include <malloc.h>
#include <string>

#include "config/logout_config.h"

#include "core/log.h"
#include "core/spawn.h"

#include "modules/logout.h"
#include "modules/logout/layout.h"

#include "render/draw.h"

namespace astralia {

namespace {

namespace cfg = logout_config;

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
    : x_(x), window_(x, "astralia-logout", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE),
      keyboard_(x.conn()), glyph_(cfg::glyph_font) {
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
    ipc.add({"logout",
             [this] {
                 toggle();
                 return std::string();
             },
             "toggle the logout overlay"});
}

void Logout::toggle() {
    if (open_) {
        close();
    } else {
        open();
    }
}

void Logout::open() {
    window_.place(x_.pointer_output());
    keyboard_.reload();
    logo_ = load_logo();
    selected_ = 0;
    hovered_ = -1;
    open_ = true;
    paint();
    window_.show(true);
    log::info("logout: open");
}

void Logout::close() {
    window_.hide();
    open_ = false;
    hovered_ = -1;
    logo_.reset();
    window_.release();
    malloc_trim(0);
}

void Logout::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        if (open_) {
            window_.present();
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
    return logout_button_at({x, y}, {window_.geometry().width / 2.0, window_.geometry().height / 2.0});
}

void Logout::paint() {
    cairo_t *cr = window_.cr();
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    Point center{window_.geometry().width / 2.0, window_.geometry().height / 2.0};
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
    window_.present();
}

} // namespace astralia
