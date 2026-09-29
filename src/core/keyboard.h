#pragma once

#include <memory>
#include <string>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

namespace astralia {

enum class KeyKind { none,
                     text,
                     backspace,
                     up,
                     down,
                     left,
                     right,
                     enter,
                     escape };

struct KeyEvent {
    KeyKind kind = KeyKind::none;
    std::string text;
};

class Keyboard {
  public:
    explicit Keyboard(xcb_connection_t *conn);

    bool reload();
    KeyEvent press(xcb_keycode_t keycode, uint16_t modifiers);

  private:
    struct Free {
        void operator()(xkb_context *context) const { xkb_context_unref(context); }
        void operator()(xkb_keymap *keymap) const { xkb_keymap_unref(keymap); }
        void operator()(xkb_state *state) const { xkb_state_unref(state); }
    };

    xcb_connection_t *conn_;
    int32_t device_ = -1;
    std::unique_ptr<xkb_context, Free> context_;
    std::unique_ptr<xkb_keymap, Free> keymap_;
    std::unique_ptr<xkb_state, Free> state_;
};

} // namespace astralia
