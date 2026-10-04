#include <cairo-xcb.h>
#include <cstdlib>
#include <format>
#include <utility>
#include <xcb/randr.h>

#include "core/x_connection.h"

namespace astralia {

namespace {

struct Free {
    void operator()(void *reply) const { std::free(reply); }
};

template <typename T>
std::unique_ptr<T, Free> owned(T *reply) {
    return std::unique_ptr<T, Free>(reply);
}

xcb_screen_t *screen_at(const xcb_setup_t *setup, int index) {
    for (auto it = xcb_setup_roots_iterator(setup); it.rem > 0; xcb_screen_next(&it), --index) {
        if (index == 0) {
            return it.data;
        }
    }
    return nullptr;
}

xcb_visualtype_t *root_visual(xcb_screen_t *screen) {
    for (auto depth = xcb_screen_allowed_depths_iterator(screen); depth.rem > 0;
         xcb_depth_next(&depth)) {
        for (auto visual = xcb_depth_visuals_iterator(depth.data); visual.rem > 0;
             xcb_visualtype_next(&visual)) {
            if (visual.data->visual_id == screen->root_visual) {
                return visual.data;
            }
        }
    }
    return nullptr;
}

xcb_visualtype_t *find_argb_visual(xcb_screen_t *screen) {
    for (auto depth = xcb_screen_allowed_depths_iterator(screen); depth.rem > 0;
         xcb_depth_next(&depth)) {
        if (depth.data->depth != 32) {
            continue;
        }
        for (auto visual = xcb_depth_visuals_iterator(depth.data); visual.rem > 0;
             xcb_visualtype_next(&visual)) {
            if (visual.data->_class == XCB_VISUAL_CLASS_TRUE_COLOR) {
                return visual.data;
            }
        }
    }
    return nullptr;
}

void disable_cairo_shm(xcb_connection_t *conn, xcb_screen_t *screen, xcb_visualtype_t *visual) {
    cairo_surface_t *probe = cairo_xcb_surface_create(conn, screen->root, visual, 1, 1);
    cairo_xcb_device_debug_cap_xshm_version(cairo_surface_get_device(probe), -1, -1);
    cairo_surface_destroy(probe);
}

} // namespace

void XConnection::Disconnect::operator()(xcb_connection_t *conn) const {
    std::free(xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), nullptr));
    xcb_disconnect(conn);
}

void XConnection::WipeEwmh::operator()(xcb_ewmh_connection_t *ewmh) const {
    xcb_ewmh_connection_wipe(ewmh);
    delete ewmh;
}

XConnection::XConnection(ConnPtr conn, EwmhPtr ewmh, xcb_screen_t *screen, xcb_visualtype_t *visual)
    : conn_(std::move(conn)), ewmh_(std::move(ewmh)), screen_(screen), visual_(visual),
      argb_visual_(find_argb_visual(screen)) {}

std::expected<XConnection, std::string> XConnection::connect() {
    int screen_index = 0;
    ConnPtr conn(xcb_connect(nullptr, &screen_index));
    if (int error = xcb_connection_has_error(conn.get()); error != 0) {
        return std::unexpected(std::format("cannot connect to the X server (xcb error {})", error));
    }
    xcb_screen_t *screen = screen_at(xcb_get_setup(conn.get()), screen_index);
    if (screen == nullptr) {
        return std::unexpected(std::format("X server has no screen {}", screen_index));
    }
    xcb_visualtype_t *visual = root_visual(screen);
    if (visual == nullptr) {
        return std::unexpected(std::string("cannot find the root visual"));
    }
    disable_cairo_shm(conn.get(), screen, visual);
    auto ewmh = std::make_unique<xcb_ewmh_connection_t>();
    xcb_intern_atom_cookie_t *cookies = xcb_ewmh_init_atoms(conn.get(), ewmh.get());
    if (xcb_ewmh_init_atoms_replies(ewmh.get(), cookies, nullptr) == 0) {
        return std::unexpected(std::string("cannot intern EWMH atoms"));
    }
    return XConnection(std::move(conn), EwmhPtr(ewmh.release()), screen, visual);
}

xcb_atom_t XConnection::atom(std::string_view name) {
    std::string key(name);
    if (auto it = atoms_.find(key); it != atoms_.end()) {
        return it->second;
    }
    auto cookie = xcb_intern_atom(conn(), 0, static_cast<uint16_t>(name.size()), name.data());
    auto reply = owned(xcb_intern_atom_reply(conn(), cookie, nullptr));
    if (!reply) {
        return XCB_ATOM_NONE;
    }
    atoms_.emplace(std::move(key), reply->atom);
    return reply->atom;
}

bool XConnection::has_randr() const {
    const xcb_query_extension_reply_t *randr = xcb_get_extension_data(conn(), &xcb_randr_id);
    if (randr == nullptr || !randr->present) {
        return false;
    }
    auto version = owned(
        xcb_randr_query_version_reply(conn(), xcb_randr_query_version(conn(), 1, 5), nullptr));
    return version != nullptr;
}

OutputGeometry XConnection::primary_output() const {
    OutputGeometry fallback{0, 0, screen_->width_in_pixels, screen_->height_in_pixels};
    if (!has_randr()) {
        return fallback;
    }
    auto primary = owned(xcb_randr_get_output_primary_reply(
        conn(), xcb_randr_get_output_primary(conn(), root()), nullptr));
    if (!primary || primary->output == XCB_NONE) {
        return fallback;
    }
    auto output = owned(xcb_randr_get_output_info_reply(
        conn(), xcb_randr_get_output_info(conn(), primary->output, XCB_CURRENT_TIME), nullptr));
    if (!output || output->crtc == XCB_NONE) {
        return fallback;
    }
    auto crtc = owned(xcb_randr_get_crtc_info_reply(
        conn(), xcb_randr_get_crtc_info(conn(), output->crtc, XCB_CURRENT_TIME), nullptr));
    if (!crtc || crtc->width == 0 || crtc->height == 0) {
        return fallback;
    }
    return {crtc->x, crtc->y, crtc->width, crtc->height};
}

std::vector<Output> XConnection::outputs() const {
    std::vector<Output> result;
    if (!has_randr()) {
        return result;
    }
    auto resources = owned(xcb_randr_get_screen_resources_current_reply(
        conn(), xcb_randr_get_screen_resources_current(conn(), root()), nullptr));
    if (!resources) {
        return result;
    }
    const xcb_randr_output_t *ids = xcb_randr_get_screen_resources_current_outputs(resources.get());
    int count = xcb_randr_get_screen_resources_current_outputs_length(resources.get());
    for (int i = 0; i < count; ++i) {
        auto output = owned(xcb_randr_get_output_info_reply(
            conn(), xcb_randr_get_output_info(conn(), ids[i], resources->config_timestamp),
            nullptr));
        if (!output || output->crtc == XCB_NONE ||
            output->connection != XCB_RANDR_CONNECTION_CONNECTED) {
            continue;
        }
        auto crtc = owned(xcb_randr_get_crtc_info_reply(
            conn(), xcb_randr_get_crtc_info(conn(), output->crtc, resources->config_timestamp),
            nullptr));
        if (!crtc || crtc->width == 0 || crtc->height == 0) {
            continue;
        }
        const auto *name =
            reinterpret_cast<const char *>(xcb_randr_get_output_info_name(output.get()));
        result.push_back({std::string(name, xcb_randr_get_output_info_name_length(output.get())),
                          {crtc->x, crtc->y, crtc->width, crtc->height}});
    }
    return result;
}

OutputGeometry XConnection::output_containing(int x, int y) const {
    for (const Output &output : outputs()) {
        const OutputGeometry &g = output.geometry;
        if (x >= g.x && x < g.x + g.width && y >= g.y && y < g.y + g.height) {
            return g;
        }
    }
    return primary_output();
}

std::pair<int, int> XConnection::pointer_position() const {
    xcb_query_pointer_reply_t *reply =
        xcb_query_pointer_reply(conn(), xcb_query_pointer(conn(), root()), nullptr);
    if (reply == nullptr) {
        return {0, 0};
    }
    std::pair<int, int> position{reply->root_x, reply->root_y};
    free(reply);
    return position;
}

OutputGeometry XConnection::pointer_output() const {
    auto [x, y] = pointer_position();
    return output_containing(x, y);
}

} // namespace astralia
