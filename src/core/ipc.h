#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/event_loop.h"
#include "core/unique_fd.h"

namespace astralia {

struct IpcHandler {
    std::string verb;
    std::function<std::string()> fn;
    std::string description;
};

std::string format_help(std::span<const IpcHandler> handlers);

class IpcServer {
  public:
    static std::expected<std::unique_ptr<IpcServer>, std::string> create(EventLoop &loop,
                                                                         std::string path);
    ~IpcServer();
    IpcServer(const IpcServer &) = delete;
    IpcServer &operator=(const IpcServer &) = delete;

    void add(IpcHandler handler);
    std::string dispatch(const std::string &command);

  private:
    IpcServer(EventLoop &loop, std::string path, UniqueFd listen_fd);

    void accept_client();

    EventLoop &loop_;
    std::string path_;
    UniqueFd listen_fd_;
    std::vector<IpcHandler> handlers_;
};

int run_ipc_client(const std::string &path, const std::string &command);

} // namespace astralia
