#include <algorithm>
#include <cmath>
#include <numbers>

#include "render/slider.h"

namespace astralia {

namespace {
constexpr int knob_size = 12;
constexpr double focus_ring = 2.0;
} // namespace

// The knob centre travels from knob/2 to width - knob/2, so the knob never leaves the track and the fill always ends under it.
int slider_percent_at(int track_x, int track_width, int px) {
    constexpr int inset = knob_size / 2;
    int span = track_width - 2 * inset;
    if (span <= 0) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround((px - track_x - inset) * 100.0 / span)), 0, 100);
}

void draw_slider(cairo_t *cr, const PanelRect &track, int percent, bool muted, bool focused) {
    constexpr double h = panel_config::track_height;
    constexpr double knob = knob_size;
    double top = track.y + (track.h - h) / 2.0;
    set_source(cr, palette::text_alpha20);
    rounded_rect(cr, track.x, top, track.w, h, h / 2.0);
    cairo_fill(cr);
    double span = std::max(0.0, track.w - knob);
    double center = track.x + knob / 2.0 + span * std::clamp(percent, 0, 100) / 100.0;
    if (percent > 0) {
        set_source(cr, muted ? palette::text_muted : palette::accent);
        rounded_rect(cr, track.x, top, center - track.x, h, h / 2.0);
        cairo_fill(cr);
    }
    set_source(cr, palette::text);
    cairo_arc(cr, center, track.y + track.h / 2.0, knob / 2.0, 0.0, 2.0 * std::numbers::pi);
    cairo_fill(cr);
    if (focused) {
        set_source(cr, palette::accent);
        cairo_arc(cr, center, track.y + track.h / 2.0, knob / 2.0 - focus_ring, 0.0, 2.0 * std::numbers::pi);
        cairo_fill(cr);
    }
}

} // namespace astralia
