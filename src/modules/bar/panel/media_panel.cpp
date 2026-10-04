#include <algorithm>
#include <chrono>
#include <numbers>

#include "config/bar_config.h"

#include "modules/bar/panel/media_panel.h"

#include "render/icons.h"
#include "render/text.h"

namespace astralia {

namespace {

constexpr int content_height = bar_config::media_thumb_size + bar_config::media_progress_top + bar_config::media_progress_height + bar_config::media_controls_top + bar_config::media_controls_height;

PanelRect draw_button(cairo_t *cr, double x, double y, double size, const char *icon) {
    set_source(cr, palette::text_alpha08);
    cairo_arc(cr, x + size / 2.0, y + size / 2.0, size / 2.0, 0.0, 2.0 * std::numbers::pi);
    cairo_fill(cr);
    Text glyph(panel_config::icon_font);
    glyph.set(icon);
    set_source(cr, palette::text);
    glyph.draw_ink_centered(cr, x + size / 2.0, y + size / 2.0);
    return {x, y, size, size};
}

} // namespace

MediaPanel::MediaPanel(XConnection &x, EventLoop &loop, MediaService &media)
    : loop_(loop), media_(media),
      window_(x, loop, "astralia-media-panel", bar_config::panel_width, bar_config::panel_max_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] {
          art_.reset();
          art_path_.clear(); }) {
    media_.changed.connect([this] {
        if (window_.is_open()) {
            paint();
        }
    });
    poll_timer_ = loop_.add_timer(
        [this] { return window_.is_open() ? bar_config::media_poll_interval : std::chrono::milliseconds(std::chrono::hours(1)); },
        [this] {
            if (window_.is_open()) {
                media_.poll_position();
            }
        });
}

void MediaPanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    media_.poll_position();
    paint();
    window_.open((window_.output().width - window_.width()) / 2, bar_config::panel_top);
    loop_.reschedule(poll_timer_);
}

void MediaPanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        if (window_.keyboard().press(key.detail, key.state).kind == KeyKind::escape) {
            window_.close();
        }
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        std::optional<PanelHit> hit = panel_hit_at(hits_, button.event_x, button.event_y);
        if (button.detail != XCB_BUTTON_INDEX_1 || !hit) {
            break;
        }
        switch (hit->action) {
        case close_panel:
            window_.close();
            break;
        case previous:
            media_.previous();
            break;
        case play_pause:
            media_.play_pause();
            break;
        case next:
            media_.next();
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

void MediaPanel::draw_art(cairo_t *cr, double x, double y) {
    constexpr double size = bar_config::media_thumb_size;
    const MediaStatus &status = media_.status();
    std::string path = status.has_player && status.track.art_url.starts_with("file://") ? status.track.art_url.substr(7) : std::string();
    if (path != art_path_) {
        art_path_ = path;
        art_.reset();
        if (!path.empty()) {
            if (auto decoded = decode_image(path, bar_config::media_thumb_size)) {
                art_ = std::move(*decoded);
            }
        }
    }

    rounded_rect(cr, x, y, size, size, bar_config::media_thumb_radius);
    set_source(cr, palette::text_alpha08);
    cairo_fill_preserve(cr);
    if (!art_) {
        cairo_new_path(cr);
        Text glyph(panel_config::icon_font);
        glyph.set(icon::music_note);
        set_source(cr, palette::text_dim);
        glyph.draw_ink_centered(cr, x + size / 2.0, y + size / 2.0);
        return;
    }
    double width = cairo_image_surface_get_width(art_.get());
    double height = cairo_image_surface_get_height(art_.get());
    double scale = std::max(size / width, size / height);
    cairo_save(cr);
    cairo_clip(cr);
    cairo_translate(cr, x + (size - width * scale) / 2.0, y + (size - height * scale) / 2.0);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, art_.get(), 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}

void MediaPanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    constexpr double label = panel_config::label_height;
    constexpr double thumb = bar_config::media_thumb_size;
    const MediaStatus &status = media_.status();
    double top = panel_content_top();
    int height = static_cast<int>(top + content_height + pad);
    window_.set_height(height);

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, height);
    panel_draw_header(cr, width, "Media", hits_, close_panel);
    double inner = width - 2 * pad;

    draw_art(cr, pad, top);
    double text_x = pad + thumb + bar_config::media_title_gap;
    int text_w = static_cast<int>(std::max(20.0, inner - thumb - bar_config::media_title_gap));
    std::string title = status.has_player ? status.track.title : "No player";
    if (title.empty()) {
        title = "Unknown";
    }
    double middle = top + thumb / 2.0;
    panel_draw_text(cr, panel_config::font, title, text_x, middle - label - bar_config::media_title_spacing / 2.0, label, text_w, palette::text);
    panel_draw_text(cr, panel_config::small_font, status.has_player ? status.track.artist : "", text_x, middle + bar_config::media_title_spacing / 2.0, label, text_w, palette::text_dim);

    double progress_y = top + thumb + bar_config::media_progress_top;
    if (status.has_player) {
        std::string progress = media_format_position(status.track.position_us) + " / " + media_format_position(status.track.length_us);
        int progress_w = panel_text_width(panel_config::small_font, progress);
        panel_draw_text(cr, panel_config::small_font, progress, pad + (inner - progress_w) / 2.0, progress_y, bar_config::media_progress_height, 0, palette::text_dim);
    }

    constexpr double side = bar_config::media_side_button;
    constexpr double play = bar_config::media_play_button;
    constexpr double spacing = bar_config::media_controls_spacing;
    double controls_y = progress_y + bar_config::media_progress_height + bar_config::media_controls_top;
    double side_y = controls_y + (bar_config::media_controls_height - side) / 2.0;
    double play_y = controls_y + (bar_config::media_controls_height - play) / 2.0;
    double x = pad + (inner - 2 * side - play - 2 * spacing) / 2.0;
    hits_.push_back({draw_button(cr, x, side_y, side, icon::player_prev), previous, {}});
    x += side + spacing;
    const char *play_icon = status.playback == MediaPlayback::playing ? icon::player_pause : icon::player_play;
    hits_.push_back({draw_button(cr, x, play_y, play, play_icon), play_pause, {}});
    x += play + spacing;
    hits_.push_back({draw_button(cr, x, side_y, side, icon::player_next), next, {}});
    window_.present();
}

} // namespace astralia
