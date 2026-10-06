#include <csignal>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "app/services.h"

#include "core/allocator.h"
#include "core/cli.h"
#include "core/daemon.h"
#include "core/event_loop.h"
#include "core/ipc.h"
#include "core/log.h"
#include "core/runtime_paths.h"
#include "core/single_instance.h"
#include "core/x_connection.h"

#include "modules/bar_set.h"
#include "modules/launcher.h"
#include "modules/logout.h"
#include "modules/notification.h"
#include "modules/osd.h"
#include "modules/overview.h"
#include "modules/polkit.h"
#include "modules/settings.h"
#include "modules/wallpaper.h"

int main(int argc, char **argv) {
    astralia::tune_allocator();
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
    astralia::Services services(*x, *loop);
    astralia::Wallpaper wallpaper(*x, services);
    astralia::BarSet bars(*x, *loop, **ipc, services);
    astralia::Launcher launcher(*x, *loop, **ipc);
    astralia::Logout logout(*x, *loop, **ipc);
    astralia::Overview overview(*x, *loop, **ipc, services);
    astralia::Polkit polkit(*x, *loop, services);
    astralia::Notifications notifications(*x, *loop, services);
    astralia::Osd osd(*x, *loop, services);
    astralia::Settings settings(*x, *loop, **ipc, services);
    return loop->run();
}
