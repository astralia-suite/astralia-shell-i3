#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/log.h"

#include "modules/launcher/search_process.h"

extern char **environ;

namespace astralia {

SearchProcess::~SearchProcess() { cancel(); }

bool SearchProcess::start(const std::vector<std::string> &argv) {
    cancel();
    std::array<int, 2> pipe_fds{};
    if (pipe2(pipe_fds.data(), O_CLOEXEC) != 0) {
        log::error("launcher: pipe failed");
        return false;
    }
    UniqueFd read_end(pipe_fds[0]);
    UniqueFd write_end(pipe_fds[1]);
    fcntl(read_end.get(), F_SETFL, O_NONBLOCK);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, write_end.get(), STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    sigset_t none;
    sigemptyset(&none);
    sigset_t defaults;
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    posix_spawnattr_setsigmask(&attr, &none);
    posix_spawnattr_setsigdefault(&attr, &defaults);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

    std::vector<char *> args;
    for (const std::string &arg : argv) {
        args.push_back(const_cast<char *>(arg.c_str()));
    }
    args.push_back(nullptr);
    pid_t pid = -1;
    int result = posix_spawnp(&pid, args[0], &actions, &attr, args.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    if (result != 0) {
        log::error("launcher: cannot run {}", argv.front());
        return false;
    }
    pid_ = pid;
    out_ = std::move(read_end);
    buffer_.clear();
    return true;
}

bool SearchProcess::read_available() {
    std::array<char, 4096> buf{};
    while (out_.get() >= 0) {
        ssize_t n = read(out_.get(), buf.data(), buf.size());
        if (n > 0) {
            buffer_.append(buf.data(), static_cast<std::size_t>(n));
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && errno == EAGAIN) {
            return false;
        }
        out_ = UniqueFd();
        reap();
        return true;
    }
    return true;
}

void SearchProcess::cancel() {
    if (pid_ > 0) {
        kill(pid_, SIGKILL);
        reap();
    }
    out_ = UniqueFd();
    buffer_.clear();
}

void SearchProcess::reap() {
    if (pid_ > 0) {
        waitpid(pid_, nullptr, 0);
        pid_ = -1;
    }
}

} // namespace astralia
