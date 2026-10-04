#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <xcb/shape.h>

#include "config/osd_config.h"

#include "modules/osd.h"

#include "render/draw.h"
#include "render/icons.h"

namespace astralia {

namespace {

namespace cfg = osd_config;

} // namespace

Osd::Osd(XConnection &x, EventLoop &loop, Services &services)
    : x_(x), loop_(loop), services_(services), window_(x, "astralia-osd", XCB_EVENT_MASK_EXPOSURE),
      icon_(cfg::icon_font), label_(cfg::label_font),
      ready_at_(std::chrono::steady_clock::now() + cfg::ready_delay) {
    label_.ellipsize(static_cast<int>(cfg::label_width));
    xcb_shape_rectangles(x_.conn(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED,
                         window_.id(), 0, 0, 0, nullptr);
    loop_.on_window(window_.id(), [this](const xcb_generic_event_t &event) {
        if (window_.mapped() && (event.response_type & ~0x80) == XCB_EXPOSE) {
            window_.present();
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

void Osd::show(Kind kind, int percent, bool muted) {
    auto now = std::chrono::steady_clock::now();
    if (now < ready_at_) {
        return;
    }
    const Output &target = services_.outputs.at_pointer();
    if (!services_.settings.enabled(Feature::osd, target.name)) {
        return;
    }
    hide_at_ = now + cfg::visible_for;
    const OutputGeometry &output = target.geometry;
    window_.place({static_cast<int16_t>(output.x + (output.width - cfg::width) / 2),
                   static_cast<int16_t>(output.y + output.height - cfg::margin_bottom - cfg::height),
                   cfg::width, cfg::height});
    paint(kind, percent, muted);
    window_.show(false);
    loop_.reschedule(timer_);
}

void Osd::hide() {
    if (!window_.mapped() || std::chrono::steady_clock::now() < hide_at_) {
        return;
    }
    window_.hide();
}

std::chrono::milliseconds Osd::until_hide() const {
    if (!window_.mapped()) {
        return std::chrono::hours(1);
    }
    auto left = hide_at_ - std::chrono::steady_clock::now();
    return std::max(std::chrono::ceil<std::chrono::milliseconds>(left), std::chrono::milliseconds(0));
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

    cairo_t *cr = window_.cr();
    window_.clear();

    constexpr double inset = cfg::border_width / 2.0;
    rounded_rect(cr, 0, 0, cfg::width, cfg::height, cfg::radius);
    set_source(cr, palette::overlay);
    cairo_fill(cr);
    rounded_rect(cr, inset, inset, cfg::width - cfg::border_width, cfg::height - cfg::border_width,
                 cfg::radius - inset);
    set_source(cr, palette::electro);
    cairo_set_line_width(cr, cfg::border_width);
    cairo_stroke(cr);

    icon_.set(glyph);
    set_source(cr, muted ? palette::text_muted : palette::text);
    icon_.draw_ink_left(cr, cfg::content_margin, cfg::height / 2.0);

    constexpr double track_x = cfg::content_margin + cfg::icon_size + cfg::bar_margin;
    constexpr double track_w =
        cfg::width - track_x - cfg::bar_margin - cfg::label_width - cfg::content_margin;
    constexpr double track_y = (cfg::height - cfg::track_height) / 2.0;
    rounded_rect(cr, track_x, track_y, track_w, cfg::track_height, cfg::track_height / 2.0);
    set_source(cr, palette::text_alpha11);
    cairo_fill(cr);
    double fill_w = track_w * std::clamp(percent, 0, 100) / 100.0;
    if (fill_w > 0.0) {
        rounded_rect(cr, track_x, track_y, fill_w, cfg::track_height, cfg::track_height / 2.0);
        set_source(cr, muted ? palette::text_muted : palette::accent);
        cairo_fill(cr);
    }

    label_.set(muted ? std::string("muted") : std::format("{}%", percent));
    set_source(cr, palette::text);
    label_.draw_centered(cr, cfg::width - cfg::content_margin - label_.width(), 0, cfg::height);
    window_.present();
}

} // namespace astralia
