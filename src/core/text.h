#pragma once

#include <cairo.h>
#include <memory>
#include <pango/pangocairo.h>
#include <string>
#include <string_view>

namespace astralia {

class Text {
  public:
    explicit Text(const char *font);

    bool set(std::string_view text);
    int width() const;
    int height() const;
    void draw(cairo_t *cr, double x, double y) const;
    void draw_centered(cairo_t *cr, double x, int top, int height) const;
    void draw_ink_centered(cairo_t *cr, double cx, double cy) const;

  private:
    struct Unref {
        void operator()(PangoLayout *layout) const;
    };

    std::unique_ptr<PangoLayout, Unref> layout_;
    std::string text_;
};

} // namespace astralia
