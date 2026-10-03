#include "render/widget_capsule.h"

namespace astralia {

WidgetCapsule::WidgetCapsule(const CapsuleStyle &style, LabelMode mode)
    : icon_(style.icon_font), label_(style.label_font), label_gap_(style.label_gap), mode_(mode) {}

bool WidgetCapsule::set_hovered(bool hovered) {
    if (hovered == hovered_) {
        return false;
    }
    hovered_ = hovered;
    return true;
}

bool WidgetCapsule::set_pinned(bool pinned) {
    if (pinned == pinned_) {
        return false;
    }
    pinned_ = pinned;
    return true;
}

bool WidgetCapsule::label_shown() const {
    return label_.width() > 0 && (mode_ == LabelMode::always || hovered_ || pinned_);
}

int WidgetCapsule::gap() const { return icon_.width() > 0 ? label_gap_ : 0; }

int WidgetCapsule::width() const {
    return icon_.width() + (label_shown() ? gap() + label_.width() : 0);
}

void WidgetCapsule::draw(cairo_t *cr, double x, int top, int height) const {
    icon_.draw_centered(cr, x, top, height);
    if (label_shown()) {
        label_.draw_centered(cr, x + icon_.width() + gap(), top, height);
    }
}

void WidgetCapsule::draw_ink_centered(cairo_t *cr, double cx, double cy) const {
    label_.draw_ink_centered(cr, cx, cy);
}

} // namespace astralia
