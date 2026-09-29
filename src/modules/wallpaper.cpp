#include <array>
#include <cairo-xcb.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <malloc.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/inotify.h>
#include <unistd.h>
#include <xcb/randr.h>

#include "config/wallpaper_config.h"

#include "core/log.h"

#include "modules/wallpaper.h"
#include "modules/wallpaper/config_file.h"
#include "modules/wallpaper/image.h"

namespace astralia {

namespace {

std::string_view env(const char *name) {
    const char *value = std::getenv(name);
    return value != nullptr ? value : "";
}

WallpaperFile read_wallpaper_file(const std::string &path) {
    std::ifstream stream(path);
    if (!stream) {
        log::info("no wallpaper config at {}", path);
        return {};
    }
    std::string text(std::istreambuf_iterator<char>(stream), {});
    WallpaperFile file = parse_wallpaper_file(text, env("HOME"));
    for (std::size_t line : file.invalid_lines) {
        log::error("{}:{}: expected `output = image`", path, line);
    }
    return file;
}

std::optional<xcb_pixmap_t> root_pixmap(XConnection &x, std::string_view atom) {
    auto cookie = xcb_get_property(x.conn(), 0, x.root(), x.atom(atom), XCB_ATOM_PIXMAP, 0, 1);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(x.conn(), cookie, nullptr);
    std::optional<xcb_pixmap_t> pixmap;
    if (reply != nullptr && xcb_get_property_value_length(reply) == sizeof(xcb_pixmap_t)) {
        pixmap = *static_cast<xcb_pixmap_t *>(xcb_get_property_value(reply));
    }
    std::free(reply);
    return pixmap;
}

void set_root_pixmap(XConnection &x, std::string_view atom, xcb_pixmap_t pixmap) {
    xcb_change_property(x.conn(), XCB_PROP_MODE_REPLACE, x.root(), x.atom(atom), XCB_ATOM_PIXMAP,
                        32, 1, &pixmap);
}

void paint_output(cairo_t *cr, const Output &output, const WallpaperFile &file) {
    std::optional<std::string> path = image_for(file, output.name);
    if (!path) {
        log::info("no wallpaper for output {}", output.name);
        return;
    }
    auto image = load_image(*path);
    if (!image) {
        log::error("wallpaper {} for {}: {}", *path, output.name, image.error());
        return;
    }
    cairo_surface_t *surface = image->get();
    const OutputGeometry &area = output.geometry;
    Placement placement = cover(cairo_image_surface_get_width(surface),
                                cairo_image_surface_get_height(surface), area.width, area.height);
    cairo_save(cr);
    cairo_rectangle(cr, area.x, area.y, area.width, area.height);
    cairo_clip(cr);
    cairo_translate(cr, area.x + placement.x, area.y + placement.y);
    cairo_scale(cr, placement.scale, placement.scale);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);
}

void draw(XConnection &x, xcb_pixmap_t pixmap, const std::vector<Output> &outputs,
          const WallpaperFile &file) {
    xcb_screen_t *screen = x.screen();
    cairo_surface_t *surface = cairo_xcb_surface_create(
        x.conn(), pixmap, x.visual(), screen->width_in_pixels, screen->height_in_pixels);
    cairo_t *cr = cairo_create(surface);
    const Color &fallback = wallpaper_config::fallback;
    cairo_set_source_rgb(cr, fallback.r, fallback.g, fallback.b);
    cairo_paint(cr);
    for (const Output &output : outputs) {
        paint_output(cr, output, file);
    }
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
    malloc_trim(0);
}

} // namespace

Wallpaper::Wallpaper(XConnection &x, EventLoop &loop)
    : x_(x), config_path_(wallpaper_file_path(env("XDG_CONFIG_HOME"), env("HOME"))) {
    apply(true);
    watch_config(loop);
    const xcb_query_extension_reply_t *randr = xcb_get_extension_data(x_.conn(), &xcb_randr_id);
    if (randr == nullptr || !randr->present) {
        return;
    }
    xcb_randr_select_input(x_.conn(), x_.root(), XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    loop.on_event(randr->first_event + XCB_RANDR_SCREEN_CHANGE_NOTIFY,
                  [this](const xcb_generic_event_t &) { apply(false); });
}

Wallpaper::~Wallpaper() {
    if (pixmap_ == XCB_NONE) {
        return;
    }
    xcb_connection_t *conn = x_.conn();
    if (root_pixmap(x_, "_XROOTPMAP_ID") == pixmap_) {
        xcb_delete_property(conn, x_.root(), x_.atom("_XROOTPMAP_ID"));
        uint32_t black = x_.screen()->black_pixel;
        xcb_change_window_attributes(conn, x_.root(), XCB_CW_BACK_PIXEL, &black);
        xcb_clear_area(conn, 0, x_.root(), 0, 0, 0, 0);
    }
    xcb_free_pixmap(conn, pixmap_);
    xcb_flush(conn);
}

void Wallpaper::watch_config(EventLoop &loop) {
    std::filesystem::path path(config_path_);
    inotify_ = UniqueFd(inotify_init1(IN_NONBLOCK | IN_CLOEXEC));
    if (inotify_.get() < 0 ||
        inotify_add_watch(inotify_.get(), path.parent_path().c_str(),
                          IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE) < 0) {
        log::error("cannot watch {}: {}", path.parent_path().string(), std::strerror(errno));
        return;
    }
    config_name_ = path.filename().string();
    loop.on_fd(inotify_.get(), [this] {
        if (config_changed()) {
            log::info("reloading {}", config_path_);
            apply(true);
        }
    });
}

bool Wallpaper::config_changed() {
    alignas(inotify_event) std::array<char, 4096> buffer;
    bool changed = false;
    ssize_t length = 0;
    while ((length = read(inotify_.get(), buffer.data(), buffer.size())) > 0) {
        for (ssize_t offset = 0; offset < length;) {
            const auto *event = reinterpret_cast<const inotify_event *>(buffer.data() + offset);
            if (event->len > 0 && config_name_ == event->name) {
                changed = true;
            }
            offset += sizeof(inotify_event) + event->len;
        }
    }
    return changed;
}

void Wallpaper::apply(bool force) {
    std::vector<Output> outputs = x_.outputs();
    if (outputs.empty()) {
        xcb_screen_t *screen = x_.screen();
        outputs.push_back({"", {0, 0, screen->width_in_pixels, screen->height_in_pixels}});
    }
    if (!force && outputs == applied_) {
        return;
    }
    applied_ = std::move(outputs);
    xcb_connection_t *conn = x_.conn();
    xcb_screen_t *screen = x_.screen();
    xcb_pixmap_t pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, screen->root_depth, pixmap, x_.root(), screen->width_in_pixels,
                      screen->height_in_pixels);
    draw(x_, pixmap, applied_, read_wallpaper_file(config_path_));

    xcb_change_window_attributes(conn, x_.root(), XCB_CW_BACK_PIXMAP, &pixmap);
    xcb_clear_area(conn, 0, x_.root(), 0, 0, 0, 0);
    set_root_pixmap(x_, "_XROOTPMAP_ID", pixmap);
    xcb_delete_property(conn, x_.root(), x_.atom("ESETROOT_PMAP_ID"));
    if (pixmap_ != XCB_NONE) {
        xcb_free_pixmap(conn, pixmap_);
    }
    pixmap_ = pixmap;
    xcb_flush(conn);
}

} // namespace astralia
