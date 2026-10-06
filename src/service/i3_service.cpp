#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <xcb/xcb_ewmh.h>

#include "config/bar_config.h"

#include "core/log.h"
#include "core/unique_fd.h"

#include "service/i3_service.h"

namespace astralia {

I3Service::I3Service(XConnection &x, EventLoop &loop) : x_(x) {
    uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(x_.conn(), x_.root(), XCB_CW_EVENT_MASK, &mask);
    refresh();
    loop.on_window(x_.root(), [this](const xcb_generic_event_t &event) {
        const auto &property = reinterpret_cast<const xcb_property_notify_event_t &>(event);
        xcb_ewmh_connection_t *ewmh = x_.ewmh();
        if (property.atom == ewmh->_NET_CLIENT_LIST) {
            windows_changed.emit();
            return;
        }
        if (property.atom != ewmh->_NET_CURRENT_DESKTOP &&
            property.atom != ewmh->_NET_DESKTOP_NAMES) {
            return;
        }
        if (refresh()) {
            changed.emit();
        }
    });
}

void I3Service::switch_to(uint32_t index) {
    if (index >= status_.count || index == status_.current) {
        return;
    }
    i3_command(std::format("workspace number {}", index + 1));
}

std::string I3Service::i3_socket_path() {
    if (const char *env = std::getenv("I3_SOCK")) {
        return env;
    }
    auto cookie = xcb_get_property(x_.conn(), 0, x_.root(), x_.atom("I3_SOCKET_PATH"),
                                   XCB_GET_PROPERTY_TYPE_ANY, 0, 256);
    std::unique_ptr<xcb_get_property_reply_t, decltype(&std::free)> reply(
        xcb_get_property_reply(x_.conn(), cookie, nullptr), &std::free);
    if (!reply) {
        return {};
    }
    return {static_cast<const char *>(xcb_get_property_value(reply.get())),
            static_cast<std::size_t>(xcb_get_property_value_length(reply.get()))};
}

namespace {

constexpr uint32_t i3_run_command = 0;
constexpr uint32_t i3_get_tree = 4;
constexpr std::size_t i3_header_size = 14;

bool read_exact(int fd, char *buffer, std::size_t size) {
    while (size > 0) {
        ssize_t got = recv(fd, buffer, size, 0);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got <= 0) {
            return false;
        }
        buffer += got;
        size -= static_cast<std::size_t>(got);
    }
    return true;
}

} // namespace

std::optional<std::string> I3Service::i3_request(uint32_t type, std::string_view payload, bool want_reply) {
    std::string path = i3_socket_path();
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.empty() || path.size() >= sizeof addr.sun_path) {
        log::error("i3 socket path unavailable");
        return std::nullopt;
    }
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    UniqueFd fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (fd.get() < 0 || connect(fd.get(), reinterpret_cast<sockaddr *>(&addr), sizeof addr) < 0) {
        log::error("i3 connect {}: {}", path, std::strerror(errno));
        return std::nullopt;
    }
    std::string message = "i3-ipc";
    uint32_t header[2] = {static_cast<uint32_t>(payload.size()), type};
    message.append(reinterpret_cast<const char *>(header), sizeof header);
    message.append(payload);
    if (send(fd.get(), message.data(), message.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(message.size())) {
        log::error("i3 send: {}", std::strerror(errno));
        return std::nullopt;
    }
    if (!want_reply) {
        return std::string();
    }
    char reply_header[i3_header_size];
    if (!read_exact(fd.get(), reply_header, sizeof reply_header)) {
        log::error("i3 reply header: {}", std::strerror(errno));
        return std::nullopt;
    }
    uint32_t length = 0;
    std::memcpy(&length, reply_header + 6, sizeof length);
    std::string reply(length, '\0');
    if (!read_exact(fd.get(), reply.data(), reply.size())) {
        log::error("i3 reply body: {}", std::strerror(errno));
        return std::nullopt;
    }
    return reply;
}

void I3Service::i3_command(std::string_view command) {
    i3_request(i3_run_command, command, true);
}

void I3Service::move_window(int64_t id, uint32_t workspace) {
    i3_command(std::format("[con_id={}] move container to workspace number {}", id, workspace));
}

void I3Service::kill_window(int64_t id) {
    i3_command(std::format("[con_id={}] kill", id));
}

I3Tree I3Service::query_tree() {
    std::optional<std::string> reply = i3_request(i3_get_tree, {}, true);
    if (!reply) {
        return {};
    }
    std::optional<Json> root = parse_json(*reply);
    return root ? parse_i3_tree(*root) : I3Tree{};
}

namespace {

struct Box {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

I3Window make_window(const Json &node, uint32_t workspace, const Box &box, bool floating) {
    I3Window entry;
    entry.id = static_cast<int64_t>(node.number_or("id", 0));
    if (const Json *properties = node.find("window_properties")) {
        entry.window_class = properties->string_or("class");
    }
    entry.x = box.x;
    entry.y = box.y;
    entry.width = box.width;
    entry.height = box.height;
    entry.workspace = workspace;
    entry.floating = floating;
    entry.fullscreen = node.number_or("fullscreen_mode", 0) > 0;
    return entry;
}

bool is_window(const Json &node) {
    const Json *window = node.find("window");
    return window != nullptr && window->type == Json::Type::number;
}

void place_tiled(const Json &node, const Box &box, const Box &area, uint32_t workspace, I3Tree &tree) {
    if (is_window(node)) {
        bool fullscreen = node.number_or("fullscreen_mode", 0) > 0;
        tree.windows.push_back(make_window(node, workspace, fullscreen ? area : box, false));
        return;
    }
    const Json *children = node.find("nodes");
    if (children == nullptr || children->type != Json::Type::array || children->array.empty()) {
        return;
    }
    std::string layout = node.string_or("layout");
    double fallback = 1.0 / static_cast<double>(children->array.size());
    double total = 0.0;
    for (const Json &child : children->array) {
        total += child.number_or("percent", fallback);
    }
    double offset = 0.0;
    for (const Json &child : children->array) {
        double share = child.number_or("percent", fallback) / total;
        Box inner = box;
        if (layout == "splith") {
            inner.x = box.x + offset;
            inner.width = box.width * share;
            offset += inner.width;
        } else if (layout == "splitv") {
            inner.y = box.y + offset;
            inner.height = box.height * share;
            offset += inner.height;
        }
        place_tiled(child, inner, area, workspace, tree);
    }
}

void place_floating(const Json &node, double origin_x, double origin_y, const Box &area, uint32_t workspace, I3Tree &tree) {
    const Json *rect = node.find("rect");
    if (is_window(node) && rect != nullptr) {
        bool fullscreen = node.number_or("fullscreen_mode", 0) > 0;
        Box box{rect->number_or("x", 0) - origin_x, rect->number_or("y", 0) - origin_y, rect->number_or("width", 0), rect->number_or("height", 0)};
        tree.windows.push_back(make_window(node, workspace, fullscreen ? area : box, true));
        return;
    }
    const Json *children = node.find("nodes");
    if (children == nullptr || children->type != Json::Type::array) {
        return;
    }
    for (const Json &child : children->array) {
        place_floating(child, origin_x, origin_y, area, workspace, tree);
    }
}

void collect_tree(const Json &node, I3Tree &tree) {
    if (node.string_or("type") == "workspace") {
        double number = node.number_or("num", -1);
        const Json *rect = node.find("rect");
        if (number < 1 || rect == nullptr) {
            return;
        }
        uint32_t workspace = static_cast<uint32_t>(number);
        Box area{0.0, 0.0, rect->number_or("width", 0), rect->number_or("height", 0)};
        tree.workspaces.push_back({workspace, area.width, area.height});
        place_tiled(node, area, area, workspace, tree);
        if (const Json *floating = node.find("floating_nodes"); floating != nullptr && floating->type == Json::Type::array) {
            for (const Json &child : floating->array) {
                place_floating(child, rect->number_or("x", 0), rect->number_or("y", 0), area, workspace, tree);
            }
        }
        return;
    }
    if (const Json *children = node.find("nodes"); children != nullptr && children->type == Json::Type::array) {
        for (const Json &child : children->array) {
            collect_tree(child, tree);
        }
    }
}

} // namespace

I3Tree parse_i3_tree(const Json &root) {
    I3Tree tree;
    collect_tree(root, tree);
    return tree;
}

bool I3Service::refresh() {
    xcb_ewmh_connection_t *ewmh = x_.ewmh();
    auto names_cookie = xcb_ewmh_get_desktop_names(ewmh, 0);
    auto current_cookie = xcb_ewmh_get_current_desktop(ewmh, 0);
    I3Status next{bar_config::workspace_count, bar_config::workspace_count};
    uint32_t desktop = 0;
    bool has_desktop =
        xcb_ewmh_get_current_desktop_reply(ewmh, current_cookie, &desktop, nullptr) != 0;
    xcb_ewmh_get_utf8_strings_reply_t names;
    if (xcb_ewmh_get_desktop_names_reply(ewmh, names_cookie, &names, nullptr) != 0) {
        std::string_view all(names.strings, names.strings_len);
        for (uint32_t i = 0; !all.empty(); ++i) {
            std::string_view name = all.substr(0, all.find('\0'));
            uint32_t number = 0;
            std::from_chars(name.data(), name.data() + name.size(), number);
            if (number >= 1 && number <= next.count) {
                next.occupied |= 1u << (number - 1);
                if (has_desktop && i == desktop) {
                    next.current = number - 1;
                }
            }
            all.remove_prefix(std::min(all.size(), name.size() + 1));
        }
        xcb_ewmh_get_utf8_strings_reply_wipe(&names);
    }
    if (next == status_) {
        return false;
    }
    status_ = next;
    return true;
}

} // namespace astralia
