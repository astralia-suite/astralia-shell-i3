#include "core/text.h"

namespace astralia {

void Text::Unref::operator()(PangoLayout *layout) const { g_object_unref(layout); }

Text::Text(const char *font) {
    PangoContext *context = pango_font_map_create_context(pango_cairo_font_map_get_default());
    layout_.reset(pango_layout_new(context));
    g_object_unref(context);
    PangoFontDescription *description = pango_font_description_from_string(font);
    pango_layout_set_font_description(layout_.get(), description);
    pango_font_description_free(description);
}

bool Text::set(std::string_view text) {
    if (text == text_) {
        return false;
    }
    text_ = text;
    pango_layout_set_text(layout_.get(), text_.data(), static_cast<int>(text_.size()));
    return true;
}

int Text::width() const {
    int width = 0;
    pango_layout_get_pixel_size(layout_.get(), &width, nullptr);
    return width;
}

int Text::height() const {
    int height = 0;
    pango_layout_get_pixel_size(layout_.get(), nullptr, &height);
    return height;
}

void Text::draw(cairo_t *cr, double x, double y) const {
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout_.get());
}

void Text::draw_centered(cairo_t *cr, double x, int top, int height) const {
    draw(cr, x, top + (height - this->height()) / 2.0);
}

void Text::draw_ink_centered(cairo_t *cr, double cx, double cy) const {
    PangoRectangle ink{};
    pango_layout_get_pixel_extents(layout_.get(), &ink, nullptr);
    draw(cr, cx - ink.x - ink.width / 2.0, cy - ink.y - ink.height / 2.0);
}

} // namespace astralia
