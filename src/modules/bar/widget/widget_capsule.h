#pragma once

#include <cairo.h>
#include <string_view>

#include "render/text.h"

namespace astralia {

struct CapsuleStyle {
    const char *icon_font;
    const char *label_font;
    int label_gap;
};

class WidgetCapsule {
  public:
    explicit WidgetCapsule(const CapsuleStyle &style);

    bool visible() const { return visible_; }
    bool set_hovered(bool hovered);
    bool set_pinned(bool pinned);
    int width() const;
    void draw(cairo_t *cr, double x, int top, int height) const;

  protected:
    void set_visible(bool visible) { visible_ = visible; }
    void set_icon(std::string_view icon) { icon_.set(icon); }
    bool set_label(std::string_view label) { return label_.set(label); }

  private:
    bool label_shown() const;
    int gap() const;

    Text icon_;
    Text label_;
    int label_gap_;
    bool visible_ = true;
    bool hovered_ = false;
    bool pinned_ = false;
};

} // namespace astralia
