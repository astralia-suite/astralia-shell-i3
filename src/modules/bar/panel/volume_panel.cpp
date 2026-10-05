#include <algorithm>
#include <format>
#include <numbers>
#include <string>

#include "config/bar_config.h"

#include "modules/bar/panel/volume_panel.h"

#include "render/icons.h"
#include "render/slider.h"

namespace astralia {

namespace {

struct Row {
    enum Kind { label,
                slider,
                section,
                divider,
                device } kind;
    int height;
    std::string text{};
    std::string detail{};
    AudioNode node{};
    bool selected = false;
};

void add_device_slider(std::vector<Row> &rows, const AudioService &audio, const char *title, uint32_t id) {
    std::optional<AudioNode> node = audio.node(id);
    rows.push_back({Row::label, panel_config::label_height, title, node ? node->label : "None"});
    if (node) {
        rows.push_back({Row::slider, bar_config::panel_slider_height, {}, {}, *node});
    }
}

std::vector<Row> build_rows(const AudioService &audio) {
    std::vector<Row> rows;
    add_device_slider(rows, audio, "Output", audio.sink_id());
    add_device_slider(rows, audio, "Input", audio.source_id());
    std::vector<AudioNode> streams = audio.nodes(AudioNodeKind::playback);
    if (!streams.empty()) {
        rows.push_back({Row::divider, 1});
        rows.push_back({Row::section, bar_config::panel_section_height, "Applications"});
        for (const AudioNode &stream : streams) {
            rows.push_back({Row::label, panel_config::label_height, stream.label});
            rows.push_back({Row::slider, bar_config::panel_slider_height, {}, {}, stream});
        }
    }
    rows.push_back({Row::divider, 1});
    auto add_devices = [&](const char *title, AudioNodeKind kind, uint32_t selected) {
        rows.push_back({Row::section, bar_config::panel_section_height, title});
        for (const AudioNode &node : audio.nodes(kind)) {
            rows.push_back({Row::device, panel_config::label_height + 6, node.label, {}, node, node.id == selected});
        }
    };
    add_devices("Output Device", AudioNodeKind::sink, audio.sink_id());
    add_devices("Input Device", AudioNodeKind::source, audio.source_id());
    return rows;
}

int rows_height(const std::vector<Row> &rows) {
    int height = 0;
    for (const Row &row : rows) {
        height += row.height + panel_config::row_gap;
    }
    return height;
}

const char *node_icon(const AudioNode &node) {
    if (node.kind == AudioNodeKind::source || node.kind == AudioNodeKind::capture) {
        return node.muted ? icon::mic_off : icon::mic_on;
    }
    return icon::volume_threshold(node.muted, node.percent);
}

} // namespace

VolumePanel::VolumePanel(XConnection &x, EventLoop &loop, AudioService &audio)
    : audio_(audio),
      window_(x, loop, "astralia-audio-panel", bar_config::panel_width, bar_config::panel_max_height, [this](const xcb_generic_event_t &event) { handle(event); }, [this] {
                  dragging_.reset();
                  selected_ = 0;
                  hovered_ = 0;
                  scroll_ = 0; }) {
    audio_.changed.connect([this](AudioKind) {
        if (window_.is_open()) {
            paint();
        }
    });
}

void VolumePanel::toggle() {
    if (window_.is_open()) {
        window_.close();
        return;
    }
    paint();
    window_.open(bar_config::margin_x);
}

void VolumePanel::handle(const xcb_generic_event_t &event) {
    switch (event.response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto &key = reinterpret_cast<const xcb_key_press_event_t &>(event);
        KeyKind kind = window_.keyboard().press(key.detail, key.state).kind;
        if (kind == KeyKind::escape) {
            window_.close();
        } else if (selected_ != 0 && (kind == KeyKind::left || kind == KeyKind::right)) {
            step(selected_, kind == KeyKind::right ? 1 : -1);
        }
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &button = reinterpret_cast<const xcb_button_press_event_t &>(event);
        press(button.event_x, button.event_y, button.detail);
        break;
    }
    case XCB_BUTTON_RELEASE:
        if (const auto &release = reinterpret_cast<const xcb_button_release_event_t &>(event); release.detail == XCB_BUTTON_INDEX_1 && dragging_) {
            dragging_.reset();
            std::optional<PanelHit> hit = panel_hit_at(hits_, release.event_x, release.event_y);
            hovered_ = hit && hit->action == slider ? static_cast<uint32_t>(std::stoul(hit->tag)) : 0;
            paint();
        }
        break;
    case XCB_MOTION_NOTIFY: {
        const auto &motion = reinterpret_cast<const xcb_motion_notify_event_t &>(event);
        if (dragging_) {
            set_percent(dragging_->id, slider_percent_at(static_cast<int>(dragging_->track.x), static_cast<int>(dragging_->track.w), motion.event_x));
            break;
        }
        std::optional<PanelHit> hit = panel_hit_at(hits_, motion.event_x, motion.event_y);
        hover(hit && hit->action == slider ? static_cast<uint32_t>(std::stoul(hit->tag)) : 0);
        break;
    }
    case XCB_LEAVE_NOTIFY:
        if (!dragging_) {
            hover(0);
        }
        break;
    default:
        break;
    }
}

void VolumePanel::hover(uint32_t id) {
    if (id != hovered_) {
        hovered_ = id;
        paint();
    }
}

void VolumePanel::press(int x, int y, xcb_button_t button) {
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    uint32_t id = hit && !hit->tag.empty() ? static_cast<uint32_t>(std::stoul(hit->tag)) : 0;
    if (button == XCB_BUTTON_INDEX_4 || button == XCB_BUTTON_INDEX_5) {
        int direction = button == XCB_BUTTON_INDEX_4 ? 1 : -1;
        if (hit && hit->action == slider) {
            step(id, direction * bar_config::panel_volume_step);
            return;
        }
        int next = panel_clamp_scroll(scroll_ - direction * bar_config::panel_scroll_step, content_height_, visible_height_);
        if (next != scroll_) {
            scroll_ = next;
            paint();
        }
        return;
    }
    if (button != XCB_BUTTON_INDEX_1 || !hit) {
        return;
    }
    switch (hit->action) {
    case close_panel:
        window_.close();
        return;
    case slider:
        selected_ = id;
        dragging_ = Drag{id, hit->rect};
        set_percent(id, slider_percent_at(static_cast<int>(hit->rect.x), static_cast<int>(hit->rect.w), x));
        return;
    case mute:
        if (std::optional<AudioNode> node = audio_.node(id)) {
            audio_.set_mute(id, !node->muted);
        }
        return;
    case device:
        audio_.set_default(id);
        return;
    default:
        return;
    }
}

void VolumePanel::step(uint32_t id, int delta) {
    if (std::optional<AudioNode> node = audio_.node(id)) {
        set_percent(id, std::clamp(node->percent + delta, 0, 100));
    }
}

void VolumePanel::set_percent(uint32_t id, int percent) {
    std::optional<AudioNode> node = audio_.node(id);
    if (node && node->percent != percent) {
        audio_.set_volume(id, percent);
    }
}

void VolumePanel::paint() {
    cairo_t *cr = window_.cr();
    double width = window_.width();
    constexpr double pad = panel_config::padding;
    std::vector<Row> rows = build_rows(audio_);
    double top = panel_content_top();
    content_height_ = rows_height(rows);
    int height = static_cast<int>(std::min<double>(window_.max_height(), top + content_height_ + pad));
    visible_height_ = static_cast<int>(height - top - pad);
    scroll_ = panel_clamp_scroll(scroll_, content_height_, visible_height_);
    window_.set_height(height);

    hits_.clear();
    window_.clear();
    panel_draw_card(cr, 0, 0, width, height);
    panel_draw_header(cr, width, "Volume", hits_, close_panel);

    PanelRect area{0, top, width, static_cast<double>(visible_height_)};
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.w, area.h);
    cairo_clip(cr);
    double y = top - scroll_;
    double inner = width - 2 * pad;
    for (const Row &row : rows) {
        PanelRect rect{pad, y, inner, static_cast<double>(row.height)};
        if (y + row.height > area.y && y < area.y + area.h) {
            switch (row.kind) {
            case Row::label: {
                int title_w = panel_draw_text(cr, panel_config::font, row.text, pad, y, row.height, static_cast<int>(inner), palette::text);
                if (!row.detail.empty()) {
                    double detail_x = pad + title_w + panel_config::row_gap;
                    panel_draw_text(cr, panel_config::small_font, "\xE2\x80\x94 " + row.detail, detail_x, y, row.height, static_cast<int>(pad + inner - detail_x), palette::text_dim);
                }
                break;
            }
            case Row::slider: {
                const AudioNode &node = row.node;
                std::string tag = std::to_string(node.id);
                PanelRect button = panel_draw_icon_button(cr, pad, y + (row.height - panel_config::button_size) / 2.0, node_icon(node), node.muted ? palette::text_muted : palette::text);
                hits_.push_back({panel_intersect(button, area), mute, tag});
                std::string percent = node.muted ? "muted" : std::format("{}%", node.percent);
                int percent_w = panel_text_width(panel_config::small_font, percent);
                panel_draw_text(cr, panel_config::small_font, percent, pad + inner - percent_w, y, row.height, 0, palette::text_dim);
                double track_x = button.x + button.w + panel_config::row_gap * 2;
                PanelRect track{track_x, y, pad + inner - bar_config::panel_percent_width - track_x, static_cast<double>(row.height)};
                draw_slider(cr, track, node.muted ? 0 : node.percent, node.muted, node.id == selected_ || node.id == hovered_ || (dragging_ && dragging_->id == node.id));
                hits_.push_back({panel_intersect(track, area), slider, tag});
                break;
            }
            case Row::section:
                panel_draw_section(cr, y, row.height, row.text);
                break;
            case Row::divider:
                set_source(cr, palette::text_alpha08);
                cairo_rectangle(cr, pad, y, inner, 1.0);
                cairo_fill(cr);
                break;
            case Row::device: {
                constexpr double dot = 14.0;
                double cy = y + row.height / 2.0;
                set_source(cr, row.selected ? palette::accent : palette::text_alpha20);
                cairo_arc(cr, pad + dot / 2.0, cy, dot / 2.0, 0.0, 2.0 * std::numbers::pi);
                cairo_fill(cr);
                if (row.selected) {
                    set_source(cr, palette::text);
                    cairo_arc(cr, pad + dot / 2.0, cy, dot / 4.0, 0.0, 2.0 * std::numbers::pi);
                    cairo_fill(cr);
                }
                double text_x = pad + dot + panel_config::row_gap * 2;
                panel_draw_text(cr, panel_config::font, row.text, text_x, y, row.height, static_cast<int>(pad + inner - text_x), row.selected ? palette::accent : palette::text);
                hits_.push_back({panel_intersect(rect, area), device, std::to_string(row.node.id)});
                break;
            }
            }
        }
        y += row.height + panel_config::row_gap;
    }
    cairo_restore(cr);
    window_.present();
}

} // namespace astralia
