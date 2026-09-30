#include <csignal>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "core/cli.h"
#include "core/daemon.h"
#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/log.h"
#include "core/runtime_paths.h"
#include "core/single_instance.h"
#include "core/x_connection.h"

#include "modules/bar.h"
#include "modules/launcher.h"
#include "modules/logout.h"
#include "modules/notification.h"
#include "modules/polkit.h"
#include "modules/wallpaper.h"

int main(int argc, char **argv) {
    std::signal(SIGPIPE, SIG_IGN);
    std::vector<std::string_view> args(argv + 1, argv + argc);
    astralia::Invocation invocation = astralia::parse_invocation(args);
    if (invocation.mode == astralia::Mode::client) {
        return astralia::run_ipc_client(astralia::runtime_path(".sock"), invocation.command);
    }
    auto instance = astralia::SingleInstance::acquire(astralia::runtime_path(".lock"));
    if (!instance) {
        astralia::log::error("{}", instance.error());
        return EXIT_FAILURE;
    }
    if (invocation.mode == astralia::Mode::daemon) {
        astralia::daemonize(astralia::runtime_path(".log"));
    }
    auto x = astralia::XConnection::connect();
    if (!x) {
        astralia::log::error("{}", x.error());
        return EXIT_FAILURE;
    }
    auto loop = astralia::EventLoop::create(*x);
    if (!loop) {
        astralia::log::error("{}", loop.error());
        return EXIT_FAILURE;
    }
    auto ipc = astralia::IpcServer::create(*loop, astralia::runtime_path(".sock"));
    if (!ipc) {
        astralia::log::error("{}", ipc.error());
        return EXIT_FAILURE;
    }
    astralia::Wallpaper wallpaper(*x, *loop);
    astralia::Bar bar(*x, *loop, **ipc);
    astralia::Launcher launcher(*x, *loop, **ipc);
    astralia::Logout logout(*x, *loop, **ipc);
    astralia::Polkit polkit(*x, *loop);
    astralia::Notifications notifications(*x, *loop);
    return loop->run();
}
