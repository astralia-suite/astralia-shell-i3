#include <csignal>
#include <linux/close_range.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/log.h"
#include "core/spawn.h"

namespace astralia {

void spawn_detached(const std::string &command) {
    log::info("spawn: {}", command);
    pid_t child = fork();
    if (child < 0) {
        log::error("spawn: fork failed");
        return;
    }
    if (child == 0) {
        setsid();
        if (fork() != 0) {
            _exit(0);
        }
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        std::signal(SIGPIPE, SIG_DFL);
        close_range(3, ~0U, 0);
        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }
    waitpid(child, nullptr, 0);
}

} // namespace astralia
