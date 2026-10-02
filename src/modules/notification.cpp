#include <algorithm>
#include <array>
#include <cairo-xcb.h>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <string_view>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config/notification_config.h"

#include "modules/notification.h"
#include "modules/notification/layout.h"

namespace astralia {

namespace {

namespace cfg = notification_config;

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

void panel(cairo_t *cr, double x, double y, double w, double h, const Color &border) {
    rounded_rect(cr, x, y, w, h, cfg::card_radius);
    set_source(cr, cfg::background);
    cairo_fill(cr);
    double inset = cfg::border_width / 2.0;
    rounded_rect(cr, x + inset, y + inset, w - cfg::border_width, h - cfg::border_width,
                 cfg::card_radius - inset);
    set_source(cr, border);
    cairo_set_line_width(cr, cfg::border_width);
    cairo_stroke(cr);
}

} // namespace

Notifications::Notifications(XConnection &x, EventLoop &loop, Services &services)
    : x_(x), app_(cfg::app_font), summary_(cfg::summary_font), body_(cfg::body_font),
      service_(services.notifications) {
    service_.changed.connect([this] { sync(); });
    app_.wrap(cfg::wrap_width);
    summary_.wrap(cfg::wrap_width);
    body_.wrap(cfg::wrap_width);
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
                                   XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS,
                                   colormap_};
    xcb_create_window(conn, depth_, window_, x_.root(), 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      values.data());
    using namespace std::string_view_literals;
    constexpr std::string_view name = "astralia-notification"sv;
    constexpr std::string_view wm_class = "astralia-notification\0astralia-shell\0"sv;
    xcb_ewmh_set_wm_name(x_.ewmh(), window_, name.size(), name.data());
    xcb_icccm_set_wm_class(conn, window_, wm_class.size(), wm_class.data());
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, window_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);
    loop.on_window(window_, [this](const xcb_generic_event_t &event) { handle(event); });
}

Notifications::~Notifications() {
    xcb_connection_t *conn = x_.conn();
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

void Notifications::sync() {
    xcb_connection_t *conn = x_.conn();
    const std::vector<Notification> &list = service_.list();
    if (list.empty()) {
        shown_.clear();
        heights_.clear();
        if (mapped_) {
            xcb_unmap_window(conn, window_);
            xcb_flush(conn);
            mapped_ = false;
        }
        return;
    }
    heights_.clear();
    for (const Notification &n : list) {
        heights_.push_back(measure(n));
    }
    auto skip = static_cast<std::ptrdiff_t>(list.size() - notification_fit_count(heights_));
    shown_.assign(list.begin() + skip, list.end());
    heights_.erase(heights_.begin(), heights_.begin() + skip);
    double stack_height = notification_stack_height(heights_);
    StackOrigin origin = notification_stack_origin(pointer_output(), stack_height);
    place(origin.x, origin.y, cfg::card_width, static_cast<uint16_t>(std::ceil(stack_height)));
    paint();
    uint32_t above = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, window_, XCB_CONFIG_WINDOW_STACK_MODE, &above);
    if (!mapped_) {
        xcb_map_window(conn, window_);
        mapped_ = true;
    }
    xcb_flush(conn);
}

void Notifications::place(int x, int y, uint16_t width, uint16_t height) {
    xcb_connection_t *conn = x_.conn();
    OutputGeometry target{static_cast<int16_t>(x), static_cast<int16_t>(y), width, height};
    if (width != geometry_.width || height != geometry_.height || cr_ == nullptr) {
        if (cr_ != nullptr) {
            cairo_destroy(cr_);
            cairo_surface_destroy(surface_);
            xcb_free_pixmap(conn, pixmap_);
        }
        pixmap_ = xcb_generate_id(conn);
        xcb_create_pixmap(conn, depth_, pixmap_, window_, width, height);
        surface_ = cairo_xcb_surface_create(conn, pixmap_, visual_, width, height);
        cr_ = cairo_create(surface_);
    }
    if (target != geometry_) {
        std::array<uint32_t, 4> values{static_cast<uint32_t>(x), static_cast<uint32_t>(y), width,
                                       height};
        xcb_configure_window(conn, window_,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             values.data());
    }
    geometry_ = target;
}

OutputGeometry Notifications::pointer_output() const {
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

void Notifications::handle(const xcb_generic_event_t &event) {
    if (!mapped_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        present();
        break;
    case XCB_BUTTON_PRESS: {
        const auto &press = reinterpret_cast<const xcb_button_press_event_t &>(event);
        if (auto index = notification_close_at(press.event_x, press.event_y, heights_)) {
            service_.dismiss(shown_[*index].id);
        }
        break;
    }
    default:
        break;
    }
}

double Notifications::measure(const Notification &n) {
    app_.set(n.app.empty() ? cfg::app_fallback : n.app);
    summary_.set(n.summary);
    body_.set(n.body);
    return notification_card_height(app_.height(), n.summary.empty() ? 0.0 : summary_.height(),
                                    n.body.empty() ? 0.0 : body_.height());
}

void Notifications::paint() {
    cairo_t *cr = cr_;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double y = 0.0;
    for (std::size_t i = 0; i < shown_.size(); ++i) {
        const Notification &n = shown_[i];
        panel(cr, 0.0, y, cfg::card_width, heights_[i],
              n.critical ? cfg::critical_border : cfg::border);
        double close_x = cfg::card_width - cfg::card_pad - cfg::close_size;
        double close_y = y + cfg::card_pad;
        set_source(cr, cfg::close);
        cairo_set_line_width(cr, cfg::close_line_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, close_x, close_y);
        cairo_line_to(cr, close_x + cfg::close_size, close_y + cfg::close_size);
        cairo_move_to(cr, close_x + cfg::close_size, close_y);
        cairo_line_to(cr, close_x, close_y + cfg::close_size);
        cairo_stroke(cr);
        double line_y = y + cfg::card_pad;
        set_source(cr, cfg::app);
        app_.set(n.app.empty() ? cfg::app_fallback : n.app);
        app_.draw(cr, cfg::card_pad, line_y);
        line_y += app_.height();
        if (!n.summary.empty()) {
            line_y += cfg::content_spacing;
            set_source(cr, cfg::summary);
            summary_.set(n.summary);
            summary_.draw(cr, cfg::card_pad, line_y);
            line_y += summary_.height();
        }
        if (!n.body.empty()) {
            line_y += cfg::content_spacing;
            set_source(cr, cfg::body);
            body_.set(n.body);
            body_.draw(cr, cfg::card_pad, line_y);
        }
        y += heights_[i] + cfg::spacing;
    }
    present();
}

void Notifications::present() {
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, 0, 0, 0, 0, geometry_.width, geometry_.height);
    xcb_flush(x_.conn());
}

} // namespace astralia
