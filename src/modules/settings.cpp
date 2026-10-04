#include <cstdlib>
#include <malloc.h>
#include <string>

#include "config/settings_config.h"

#include "core/log.h"

#include "modules/settings.h"

#include "render/draw.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

} // namespace

Settings::Settings(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services)
    : x_(x), services_(services),
      window_(x, "astralia-settings", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE),
      keyboard_(x.conn()), displays_(services),
      wallpaper_(services, loop, [this] {
          if (open_ && tab_ == wallpaper) {
              paint();
          }
      }) {
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
    ipc.add({"settings",
             [this] {
                 toggle();
                 return std::string();
             },
             "toggle the settings overlay"});
    services.settings.changed.connect([this] { sync(); });
    services.wallpaper.changed.connect([this] { sync(); });
    services.outputs.changed.connect([this] { sync(); });
}

void Settings::toggle() {
    if (open_) {
        close();
    } else {
        open();
    }
}

void Settings::open() {
    OutputGeometry output = x_.pointer_output();
    auto [width, height] = settings_window_size(output.width, output.height);
    window_.place({static_cast<int16_t>(output.x + (output.width - width) / 2),
                   static_cast<int16_t>(output.y + (output.height - height) / 2),
                   static_cast<uint16_t>(width), static_cast<uint16_t>(height)});
    geometry_ = settings_geometry(width, height);
    keyboard_.reload();
    open_ = true;
    if (tab_ == wallpaper) {
        wallpaper_.open();
    }
    paint();
    window_.show(true);
    log::info("settings: open");
}

void Settings::close() {
    release_grab();
    window_.hide();
    open_ = false;
    wallpaper_.close();
    window_.release();
    malloc_trim(0);
}

void Settings::select(Tab tab) {
    if (tab == tab_) {
        return;
    }
    if (tab_ == wallpaper) {
        wallpaper_.close();
    }
    tab_ = tab;
    if (tab_ == wallpaper) {
        wallpaper_.open();
    }
}

void Settings::handle(const xcb_generic_event_t &event) {
    if (!open_) {
        return;
    }
    switch (event.response_type & ~0x80) {
    case XCB_EXPOSE:
        window_.present();
        if (!grabbed_) {
            grab();
        }
        break;
    case XCB_KEY_PRESS: {
        const auto &press = reinterpret_cast<const xcb_key_press_event_t &>(event);
        key(keyboard_.press(press.detail, press.state));
        break;
    }
    case XCB_BUTTON_PRESS: {
        const auto &press = reinterpret_cast<const xcb_button_press_event_t &>(event);
        const OutputGeometry &area = window_.geometry();
        if (press.event_x < 0 || press.event_y < 0 || press.event_x >= area.width || press.event_y >= area.height) {
            close();
        } else if (press.detail == XCB_BUTTON_INDEX_1) {
            click(press.event_x, press.event_y);
        } else if (press.detail == XCB_BUTTON_INDEX_4) {
            scroll(-1);
        } else if (press.detail == XCB_BUTTON_INDEX_5) {
            scroll(1);
        }
        break;
    }
    case XCB_FOCUS_OUT: {
        const auto &focus = reinterpret_cast<const xcb_focus_out_event_t &>(event);
        if (focus.mode == XCB_NOTIFY_MODE_NORMAL && focus.detail != XCB_NOTIFY_DETAIL_POINTER) {
            close();
        }
        break;
    }
    default:
        break;
    }
}

void Settings::grab() {
    constexpr uint16_t mask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;
    xcb_grab_pointer_reply_t *reply = xcb_grab_pointer_reply(
        x_.conn(), xcb_grab_pointer(x_.conn(), 1, window_.id(), mask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME), nullptr);
    grabbed_ = reply != nullptr && reply->status == XCB_GRAB_STATUS_SUCCESS;
    std::free(reply);
}

void Settings::release_grab() {
    if (grabbed_) {
        xcb_ungrab_pointer(x_.conn(), XCB_CURRENT_TIME);
        grabbed_ = false;
    }
}

void Settings::key(const KeyEvent &event) {
    if (tab_ == wallpaper && wallpaper_.key(event)) {
        paint();
        return;
    }
    switch (event.kind) {
    case KeyKind::escape:
        close();
        return;
    case KeyKind::up:
        select(displays);
        break;
    case KeyKind::down:
        select(wallpaper);
        break;
    default:
        return;
    }
    paint();
}

void Settings::click(double x, double y) {
    if (std::optional<std::size_t> index = settings_tab_at(geometry_, x, y)) {
        if (tab_ == wallpaper) {
            wallpaper_.click(std::nullopt);
        }
        select(static_cast<Tab>(*index));
        paint();
        return;
    }
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    bool changed = tab_ == displays ? displays_.click(hit) : wallpaper_.click(hit);
    if (changed) {
        paint();
    }
}

void Settings::scroll(int direction) {
    if (tab_ == wallpaper && wallpaper_.scroll(direction)) {
        paint();
    }
}

void Settings::sync() {
    if (!open_) {
        return;
    }
    if (tab_ == wallpaper) {
        wallpaper_.refresh();
    }
    paint();
}

void Settings::paint() {
    cairo_t *cr = window_.cr();
    window_.clear();
    panel_draw_card(cr, geometry_.card.x, geometry_.card.y, geometry_.card.w, geometry_.card.h);

    panel_draw_text(cr, panel_config::title_font, "Settings", geometry_.rail.x + cfg::rail_padding + cfg::tab_text_inset, geometry_.rail.y + cfg::rail_padding, cfg::rail_title_height, 0, palette::text);
    set_source(cr, palette::text_alpha15);
    cairo_rectangle(cr, geometry_.rail.w, cfg::content_padding, 1.0, geometry_.card.h - 2 * cfg::content_padding);
    cairo_fill(cr);
    for (std::size_t i = 0; i < cfg::tab_count; ++i) {
        PanelRect rect = settings_tab_rect(geometry_, i);
        if (i == tab_) {
            set_source(cr, palette::accent_alpha25);
            rounded_rect(cr, rect.x, rect.y, rect.w, rect.h, cfg::tile_radius);
            cairo_fill(cr);
        }
        panel_draw_text(cr, panel_config::font, cfg::tab_labels[i], rect.x + cfg::tab_text_inset, rect.y, rect.h, static_cast<int>(rect.w - 2 * cfg::tab_text_inset), i == tab_ ? palette::text : palette::text_muted);
    }

    hits_.clear();
    if (tab_ == displays) {
        displays_.paint(cr, geometry_.content, hits_);
    } else {
        wallpaper_.paint(cr, geometry_.content, hits_);
    }
    window_.present();
}

} // namespace astralia
