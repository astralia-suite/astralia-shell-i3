#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

namespace astralia {

struct OutputGeometry {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;

    bool operator==(const OutputGeometry &) const = default;
};

struct Output {
    std::string name;
    OutputGeometry geometry;

    bool operator==(const Output &) const = default;
};

class XConnection {
  public:
    static std::expected<XConnection, std::string> connect();

    xcb_connection_t *conn() const { return conn_.get(); }
    xcb_screen_t *screen() const { return screen_; }
    xcb_window_t root() const { return screen_->root; }
    xcb_visualtype_t *visual() const { return visual_; }
    xcb_visualtype_t *argb_visual() const { return argb_visual_; }
    xcb_ewmh_connection_t *ewmh() const { return ewmh_.get(); }
    xcb_atom_t atom(std::string_view name);
    OutputGeometry primary_output() const;
    std::vector<Output> outputs() const;
    OutputGeometry pointer_output() const;

  private:
    struct Disconnect {
        void operator()(xcb_connection_t *conn) const;
    };
    struct WipeEwmh {
        void operator()(xcb_ewmh_connection_t *ewmh) const;
    };
    using ConnPtr = std::unique_ptr<xcb_connection_t, Disconnect>;
    using EwmhPtr = std::unique_ptr<xcb_ewmh_connection_t, WipeEwmh>;

    XConnection(ConnPtr conn, EwmhPtr ewmh, xcb_screen_t *screen, xcb_visualtype_t *visual);

    bool has_randr() const;

    ConnPtr conn_;
    EwmhPtr ewmh_;
    xcb_screen_t *screen_;
    xcb_visualtype_t *visual_;
    xcb_visualtype_t *argb_visual_;
    std::unordered_map<std::string, xcb_atom_t> atoms_;
};

} // namespace astralia
