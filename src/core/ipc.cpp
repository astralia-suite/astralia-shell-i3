#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <print>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

#include "core/ipc.h"
#include "core/log.h"

namespace astralia {

namespace {

constexpr std::string_view error_prefix = "error: ";
constexpr timeval io_timeout{.tv_sec = 2, .tv_usec = 0};

std::string errno_message(std::string_view what) {
    return std::format("{}: {}", what, std::strerror(errno));
}

std::expected<sockaddr_un, std::string> socket_address(const std::string &path) {
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof addr.sun_path) {
        return std::unexpected(std::format("socket path too long: {}", path));
    }
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    return addr;
}

void set_timeouts(int fd) {
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout, sizeof io_timeout);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout, sizeof io_timeout);
}

void write_all(int fd, std::string_view data) {
    while (!data.empty()) {
        ssize_t n = write(fd, data.data(), data.size());
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
}

} // namespace

std::string format_help(std::span<const IpcHandler> handlers) {
    std::vector<const IpcHandler *> sorted;
    std::size_t width = 0;
    for (const IpcHandler &handler : handlers) {
        sorted.push_back(&handler);
        width = std::max(width, handler.verb.size());
    }
    std::ranges::sort(sorted, {}, &IpcHandler::verb);
    std::string help = "astralia-shell <verb>:\n";
    for (const IpcHandler *handler : sorted) {
        help += std::format("  {:<{}}  {}\n", handler->verb, width, handler->description);
    }
    return help;
}

IpcServer::IpcServer(EventLoop &loop, std::string path, UniqueFd listen_fd)
    : loop_(loop), path_(std::move(path)), listen_fd_(std::move(listen_fd)) {
    add({"help", [this] { return format_help(handlers_); }, "list the available verbs"});
    add({"kill",
         [this] {
             loop_.stop(EXIT_SUCCESS);
             return std::string();
         },
         "gracefully quit astralia-shell"});
    loop_.on_fd(listen_fd_.get(), [this] { accept_client(); });
}

std::expected<std::unique_ptr<IpcServer>, std::string> IpcServer::create(EventLoop &loop,
                                                                         std::string path) {
    auto addr = socket_address(path);
    if (!addr) {
        return std::unexpected(addr.error());
    }
    unlink(path.c_str());
    UniqueFd fd(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    if (fd.get() < 0) {
        return std::unexpected(errno_message("socket"));
    }
    if (bind(fd.get(), reinterpret_cast<const sockaddr *>(&*addr), sizeof *addr) != 0) {
        return std::unexpected(errno_message(std::format("bind {}", path)));
    }
    if (listen(fd.get(), 8) != 0) {
        unlink(path.c_str());
        return std::unexpected(errno_message("listen"));
    }
    return std::unique_ptr<IpcServer>(new IpcServer(loop, std::move(path), std::move(fd)));
}

IpcServer::~IpcServer() {
    loop_.remove_fd(listen_fd_.get());
    unlink(path_.c_str());
}

void IpcServer::add(IpcHandler handler) { handlers_.push_back(std::move(handler)); }

void IpcServer::accept_client() {
    UniqueFd client(accept4(listen_fd_.get(), nullptr, nullptr, SOCK_CLOEXEC));
    if (client.get() < 0) {
        return;
    }
    set_timeouts(client.get());
    char buf[256];
    ssize_t n = read(client.get(), buf, sizeof buf);
    if (n <= 0) {
        return;
    }
    std::string command(buf, static_cast<std::size_t>(n));
    while (!command.empty() && (command.back() == '\n' || command.back() == '\r')) {
        command.pop_back();
    }
    log::info("ipc: {}", command);
    write_all(client.get(), dispatch(command));
    if (loop_.stopping()) {
        client.release();
    }
}

std::string IpcServer::dispatch(const std::string &command) {
    for (const IpcHandler &handler : handlers_) {
        if (handler.verb == command) {
            return handler.fn();
        }
    }
    log::error("ipc: unknown command '{}'", command);
    return std::format("{}unknown command '{}'\n", error_prefix, command);
}

int run_ipc_client(const std::string &path, const std::string &command) {
    auto addr = socket_address(path);
    if (!addr) {
        std::println(stderr, "astralia-shell: {}", addr.error());
        return EXIT_FAILURE;
    }
    UniqueFd fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (fd.get() < 0) {
        std::println(stderr, "astralia-shell: {}", errno_message("socket"));
        return EXIT_FAILURE;
    }
    set_timeouts(fd.get());
    if (connect(fd.get(), reinterpret_cast<const sockaddr *>(&*addr), sizeof *addr) != 0) {
        std::println(stderr, "astralia-shell: no running instance ({})", errno_message(path));
        return EXIT_FAILURE;
    }
    write_all(fd.get(), command + '\n');

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd.get(), buf, sizeof buf)) > 0) {
        response.append(buf, static_cast<std::size_t>(n));
    }
    if (response.starts_with(error_prefix)) {
        std::print(stderr, "astralia-shell: {}", response.substr(error_prefix.size()));
        return EXIT_FAILURE;
    }
    std::fwrite(response.data(), 1, response.size(), stdout);
    return EXIT_SUCCESS;
}

} // namespace astralia
