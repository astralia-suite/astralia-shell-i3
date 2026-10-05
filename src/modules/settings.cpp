#include <chrono>
#include <cstdlib>
#include <malloc.h>
#include <string>

#include "config/settings_config.h"

#include "core/log.h"

#include "modules/settings.h"

#include "modules/settings/widgets.h"

#include "render/draw.h"
#include "render/icons.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

} // namespace

Settings::Settings(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services)
    : x_(x), loop_(loop), services_(services),
      window_(x, "astralia-settings", XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE),
      keyboard_(x.conn()), bar_(services), displays_(services),
      wallpaper_(services, loop, [this] {
          if (!repaint_pending_) {
              repaint_pending_ = true;
              loop_.reschedule(repaint_timer_);
          }
      }) {
    repaint_timer_ = loop.add_timer([this] { return repaint_pending_ ? cfg::repaint_batch : cfg::repaint_idle; },
                                    [this] {
                                        if (!repaint_pending_) {
                                            return;
                                        }
                                        repaint_pending_ = false;
                                        if (open_ && tab_ == wallpaper) {
                                            paint();
                                        }
                                    });
    loop.on_window(window_.id(), [this](const xcb_generic_event_t &event) { handle(event); });
    ipc.add({"settings",
             [this] {
                 toggle();
                 return std::string();
             },
             "toggle the settings overlay"});
    services.settings.changed.connect([this] {
        refocus_until_ = std::chrono::steady_clock::now() + settings_config::relayout_grace;
        sync();
    });
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
        if (focus.mode != XCB_NOTIFY_MODE_NORMAL || focus.detail == XCB_NOTIFY_DETAIL_POINTER) {
            break;
        }
        if (std::chrono::steady_clock::now() < refocus_until_) {
            window_.focus();
        } else {
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
        select(tab_ == bar ? bar : static_cast<Tab>(tab_ - 1));
        break;
    case KeyKind::down:
        select(tab_ == wallpaper ? wallpaper : static_cast<Tab>(tab_ + 1));
        break;
    default:
        return;
    }
    paint();
}

void Settings::click(double x, double y) {
    if (geometry_.close.contains(x, y)) {
        close();
        return;
    }
    if (std::optional<std::size_t> index = settings_tab_at(geometry_, x, y)) {
        if (tab_ == wallpaper) {
            wallpaper_.click(std::nullopt);
        }
        select(static_cast<Tab>(*index));
        paint();
        return;
    }
    std::optional<PanelHit> hit = panel_hit_at(hits_, x, y);
    bool changed = tab_ == bar ? bar_.click(hit) : tab_ == displays ? displays_.click(hit)
                                                                    : wallpaper_.click(hit);
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
    auto start = std::chrono::steady_clock::now();
    cairo_t *cr = window_.cr();
    window_.clear();
    settings_draw_card(cr, geometry_.card);

    panel_draw_text(cr, panel_config::title_font, "Settings", geometry_.header.x, geometry_.header.y, geometry_.header.h, 0, palette::text);
    panel_draw_icon_button(cr, geometry_.close.x, geometry_.close.y, icon::close, palette::text);
    set_source(cr, palette::text_alpha11);
    cairo_rectangle(cr, geometry_.header.x, geometry_.header_divider_y, geometry_.header.w, 1.0);
    cairo_fill(cr);

    settings_draw_profile(cr, geometry_, services_.user.name(), services_.user.uptime());
    settings_draw_rail(cr, geometry_, tab_);
    cairo_rectangle(cr, geometry_.divider_x, geometry_.content.y, 1.0, geometry_.rail.y + geometry_.rail.h - geometry_.content.y);
    cairo_fill(cr);

    hits_.clear();
    if (tab_ == bar) {
        bar_.paint(cr, geometry_.content, hits_);
    } else if (tab_ == displays) {
        displays_.paint(cr, geometry_.content, hits_);
    } else {
        wallpaper_.paint(cr, geometry_.content, hits_);
    }
    window_.present();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    if (elapsed >= cfg::slow_paint) {
        log::info("settings: paint in {} ms", elapsed.count());
    }
}

} // namespace astralia
