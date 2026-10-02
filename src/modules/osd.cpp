#include <algorithm>
#include <array>
#include <cairo-xcb.h>
#include <chrono>
#include <cstdlib>
#include <format>
#include <numbers>
#include <string>
#include <string_view>
#include <xcb/shape.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config/osd_config.h"

#include "core/icons.h"

#include "modules/osd.h"

namespace astralia {

namespace {

namespace cfg = osd_config;

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

} // namespace

Osd::Osd(XConnection &x, EventLoop &loop, Services &services)
    : x_(x), loop_(loop), services_(services), icon_(cfg::icon_font), label_(cfg::label_font),
      ready_at_(std::chrono::steady_clock::now() + cfg::ready_delay) {
    label_.ellipsize(static_cast<int>(cfg::label_width));
    xcb_connection_t *conn = x_.conn();
    xcb_visualtype_t *visual = x_.argb_visual();
    uint8_t depth = 32;
    if (visual == nullptr) {
        visual = x_.visual();
        depth = x_.screen()->root_depth;
    }
    colormap_ = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap_, x_.root(), visual->visual_id);
    window_ = xcb_generate_id(conn);
    std::array<uint32_t, 5> values{XCB_BACK_PIXMAP_NONE, 0, 1, XCB_EVENT_MASK_EXPOSURE, colormap_};
    xcb_create_window(conn, depth, window_, x_.root(), 0, 0, cfg::width, cfg::height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual->visual_id,
                      XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      values.data());
    using namespace std::string_view_literals;
    constexpr std::string_view name = "astralia-osd"sv;
    constexpr std::string_view wm_class = "astralia-osd\0astralia-shell\0"sv;
    xcb_ewmh_set_wm_name(x_.ewmh(), window_, name.size(), name.data());
    xcb_icccm_set_wm_class(conn, window_, wm_class.size(), wm_class.data());
    xcb_shape_rectangles(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED,
                         window_, 0, 0, 0, nullptr);

    pixmap_ = xcb_generate_id(conn);
    xcb_create_pixmap(conn, depth, pixmap_, window_, cfg::width, cfg::height);
    gc_ = xcb_generate_id(conn);
    uint32_t graphics_exposures = 0;
    xcb_create_gc(conn, gc_, pixmap_, XCB_GC_GRAPHICS_EXPOSURES, &graphics_exposures);
    surface_ = cairo_xcb_surface_create(conn, pixmap_, visual, cfg::width, cfg::height);
    cr_ = cairo_create(surface_);

    loop_.on_window(window_, [this](const xcb_generic_event_t &event) {
        if (mapped_ && (event.response_type & ~0x80) == XCB_EXPOSE) {
            present();
        }
    });
    timer_ = loop_.add_timer([this] { return until_hide(); }, [this] { hide(); });
    services_.brightness.changed.connect(
        [this] { show(Kind::brightness, services_.brightness.percent(), false); });
    services_.audio.changed.connect([this](AudioKind kind) {
        AudioLevel level =
            kind == AudioKind::sink ? services_.audio.sink() : services_.audio.source();
        if (level.present) {
            show(kind == AudioKind::sink ? Kind::volume : Kind::mic, level.percent, level.muted);
        }
    });
}

Osd::~Osd() {
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
    xcb_connection_t *conn = x_.conn();
    xcb_free_gc(conn, gc_);
    xcb_free_pixmap(conn, pixmap_);
    xcb_destroy_window(conn, window_);
    xcb_free_colormap(conn, colormap_);
    xcb_flush(conn);
}

void Osd::show(Kind kind, int percent, bool muted) {
    auto now = std::chrono::steady_clock::now();
    if (now < ready_at_) {
        return;
    }
    hide_at_ = now + cfg::visible_for;
    OutputGeometry output = pointer_output();
    xcb_connection_t *conn = x_.conn();
    std::array<uint32_t, 3> values{
        static_cast<uint32_t>(output.x + (output.width - cfg::width) / 2),
        static_cast<uint32_t>(output.y + output.height - cfg::margin_bottom - cfg::height),
        XCB_STACK_MODE_ABOVE};
    xcb_configure_window(conn, window_,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE,
                         values.data());
    paint(kind, percent, muted);
    if (!mapped_) {
        xcb_map_window(conn, window_);
        mapped_ = true;
    }
    xcb_flush(conn);
    loop_.reschedule(timer_);
}

void Osd::hide() {
    if (!mapped_ || std::chrono::steady_clock::now() < hide_at_) {
        return;
    }
    xcb_unmap_window(x_.conn(), window_);
    xcb_flush(x_.conn());
    mapped_ = false;
}

std::chrono::milliseconds Osd::until_hide() const {
    if (!mapped_) {
        return std::chrono::hours(1);
    }
    auto left = hide_at_ - std::chrono::steady_clock::now();
    return std::max(std::chrono::ceil<std::chrono::milliseconds>(left), std::chrono::milliseconds(0));
}

OutputGeometry Osd::pointer_output() const {
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

void Osd::paint(Kind kind, int percent, bool muted) {
    const char *glyph = nullptr;
    switch (kind) {
    case Kind::brightness:
        glyph = icon::brightness_threshold(percent);
        break;
    case Kind::mic:
        glyph = muted ? icon::mic_off : icon::mic_on;
        break;
    case Kind::volume:
        glyph = icon::volume_threshold(muted, percent);
        break;
    }

    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr_, 0, 0, 0, 0);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);

    constexpr double inset = cfg::border_width / 2.0;
    rounded_rect(cr_, 0, 0, cfg::width, cfg::height, cfg::radius);
    set_source(cr_, cfg::background);
    cairo_fill(cr_);
    rounded_rect(cr_, inset, inset, cfg::width - cfg::border_width, cfg::height - cfg::border_width,
                 cfg::radius - inset);
    set_source(cr_, cfg::border);
    cairo_set_line_width(cr_, cfg::border_width);
    cairo_stroke(cr_);

    icon_.set(glyph);
    set_source(cr_, muted ? cfg::muted : cfg::icon);
    icon_.draw_ink_left(cr_, cfg::content_margin, cfg::height / 2.0);

    constexpr double track_x = cfg::content_margin + cfg::icon_size + cfg::bar_margin;
    constexpr double track_w =
        cfg::width - track_x - cfg::bar_margin - cfg::label_width - cfg::content_margin;
    constexpr double track_y = (cfg::height - cfg::track_height) / 2.0;
    rounded_rect(cr_, track_x, track_y, track_w, cfg::track_height, cfg::track_height / 2.0);
    set_source(cr_, cfg::track);
    cairo_fill(cr_);
    double fill_w = track_w * std::clamp(percent, 0, 100) / 100.0;
    if (fill_w > 0.0) {
        rounded_rect(cr_, track_x, track_y, fill_w, cfg::track_height, cfg::track_height / 2.0);
        set_source(cr_, muted ? cfg::muted : cfg::fill);
        cairo_fill(cr_);
    }

    label_.set(muted ? std::string("muted") : std::format("{}%", percent));
    set_source(cr_, cfg::label);
    label_.draw_centered(cr_, cfg::width - cfg::content_margin - label_.width(), 0, cfg::height);
    present();
}

void Osd::present() {
    cairo_surface_flush(surface_);
    xcb_copy_area(x_.conn(), pixmap_, window_, gc_, 0, 0, 0, 0, cfg::width, cfg::height);
    xcb_flush(x_.conn());
}

} // namespace astralia
