#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <format>
#include <memory>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>
#include <xcb/xcb_ewmh.h>

#include "config/bar_config.h"

#include "core/log.h"

#include "service/workspace_service.h"

namespace astralia {

WorkspaceService::WorkspaceService(XConnection &x, EventLoop &loop, std::function<void()> on_change)
    : x_(x), on_change_(std::move(on_change)) {
    uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(x_.conn(), x_.root(), XCB_CW_EVENT_MASK, &mask);
    refresh();
    loop.on_window(x_.root(), [this](const xcb_generic_event_t &event) {
        const auto &property = reinterpret_cast<const xcb_property_notify_event_t &>(event);
        xcb_ewmh_connection_t *ewmh = x_.ewmh();
        if (property.atom != ewmh->_NET_CURRENT_DESKTOP &&
            property.atom != ewmh->_NET_DESKTOP_NAMES) {
            return;
        }
        if (refresh()) {
            on_change_();
        }
    });
}

void WorkspaceService::switch_to(uint32_t index) {
    if (index >= status_.count || index == status_.current) {
        return;
    }
    i3_command(std::format("workspace number {}", index + 1));
}

std::string WorkspaceService::i3_socket_path() {
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

void WorkspaceService::i3_command(std::string_view command) {
    std::string path = i3_socket_path();
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.empty() || path.size() >= sizeof addr.sun_path) {
        log::error("i3 socket path unavailable");
        return;
    }
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0 || connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) < 0) {
        log::error("i3 connect {}: {}", path, std::strerror(errno));
        if (fd >= 0) {
            close(fd);
        }
        return;
    }
    std::string message = "i3-ipc";
    uint32_t header[2] = {static_cast<uint32_t>(command.size()), 0};
    message.append(reinterpret_cast<const char *>(header), sizeof header);
    message.append(command);
    if (send(fd, message.data(), message.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(message.size())) {
        log::error("i3 send: {}", std::strerror(errno));
    }
    close(fd);
}

bool WorkspaceService::refresh() {
    xcb_ewmh_connection_t *ewmh = x_.ewmh();
    auto names_cookie = xcb_ewmh_get_desktop_names(ewmh, 0);
    auto current_cookie = xcb_ewmh_get_current_desktop(ewmh, 0);
    WorkspaceStatus next{bar_config::workspace_count, bar_config::workspace_count};
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
