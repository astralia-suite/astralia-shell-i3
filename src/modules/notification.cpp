#include <algorithm>
#include <cmath>

#include "config/notification_config.h"

#include "modules/notification.h"
#include "modules/notification/layout.h"

#include "render/draw.h"

namespace astralia {

namespace {

namespace cfg = notification_config;

void panel(cairo_t *cr, double x, double y, double w, double h, const Color &border) {
    rounded_rect(cr, x, y, w, h, cfg::card_radius);
    set_source(cr, palette::overlay);
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
    : services_(services), window_(x, "astralia-notification", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS),
      app_(cfg::app_font), summary_(cfg::summary_font), body_(cfg::body_font),
      service_(services.notifications) {
    service_.changed.connect([this] { sync(); });
    services.outputs.changed.connect([this] { sync(); });
    services.settings.changed.connect([this] { sync(); });
    app_.wrap(cfg::wrap_width);
    summary_.wrap(cfg::wrap_width);
    body_.wrap(cfg::wrap_width);
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
}

void Notifications::sync() {
    const std::vector<Notification> &list = service_.list();
    const Output &target = services_.outputs.at_pointer();
    if (list.empty() || !services_.settings.enabled(Feature::notifications, target.name)) {
        shown_.clear();
        heights_.clear();
        window_.hide();
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
    StackOrigin origin = notification_stack_origin(target.geometry, stack_height);
    window_.place({static_cast<int16_t>(origin.x), static_cast<int16_t>(origin.y), cfg::card_width,
                   static_cast<uint16_t>(std::ceil(stack_height))});
    paint();
    window_.show(false);
}

void Notifications::handle(const xcb_generic_event_t &event) {
    if (!window_.mapped()) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        window_.present();
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
    cairo_t *cr = window_.cr();
    window_.clear();

    double y = 0.0;
    for (std::size_t i = 0; i < shown_.size(); ++i) {
        const Notification &n = shown_[i];
        panel(cr, 0.0, y, cfg::card_width, heights_[i],
              n.critical ? palette::critical : palette::accent);
        double close_x = cfg::card_width - cfg::card_pad - cfg::close_size;
        double close_y = y + cfg::card_pad;
        set_source(cr, palette::text_muted);
        cairo_set_line_width(cr, cfg::close_line_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, close_x, close_y);
        cairo_line_to(cr, close_x + cfg::close_size, close_y + cfg::close_size);
        cairo_move_to(cr, close_x + cfg::close_size, close_y);
        cairo_line_to(cr, close_x, close_y + cfg::close_size);
        cairo_stroke(cr);
        double line_y = y + cfg::card_pad;
        set_source(cr, palette::accent);
        app_.set(n.app.empty() ? cfg::app_fallback : n.app);
        app_.draw(cr, cfg::card_pad, line_y);
        line_y += app_.height();
        if (!n.summary.empty()) {
            line_y += cfg::content_spacing;
            set_source(cr, palette::text);
            summary_.set(n.summary);
            summary_.draw(cr, cfg::card_pad, line_y);
            line_y += summary_.height();
        }
        if (!n.body.empty()) {
            line_y += cfg::content_spacing;
            set_source(cr, palette::text_muted);
            body_.set(n.body);
            body_.draw(cr, cfg::card_pad, line_y);
        }
        y += heights_[i] + cfg::spacing;
    }
    window_.present();
}

} // namespace astralia
