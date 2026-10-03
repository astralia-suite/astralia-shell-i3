#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "core/async_process.h"
#include "core/log.h"

extern char **environ;

namespace astralia {

AsyncProcess::AsyncProcess(EventLoop &loop) : loop_(loop), shared_(std::make_shared<Shared>()) {
    shared_->wake = UniqueFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (shared_->wake.get() < 0) {
        log::error("async_process: eventfd failed");
        return;
    }
    loop_.on_fd(shared_->wake.get(), [this] { finish(); });
}

AsyncProcess::~AsyncProcess() {
    cancel();
    if (shared_->wake.get() >= 0) {
        loop_.remove_fd(shared_->wake.get());
    }
}

bool AsyncProcess::start(const std::vector<std::string> &argv, Done done, bool merge_stderr) {
    cancel();
    if (argv.empty() || shared_->wake.get() < 0) {
        return false;
    }
    std::array<int, 2> pipe_fds{};
    if (pipe2(pipe_fds.data(), O_CLOEXEC) != 0) {
        log::error("async_process: pipe failed");
        return false;
    }
    UniqueFd read_end(pipe_fds[0]);
    UniqueFd write_end(pipe_fds[1]);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, write_end.get(), STDOUT_FILENO);
    if (merge_stderr) {
        posix_spawn_file_actions_adddup2(&actions, write_end.get(), STDERR_FILENO);
    } else {
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    }
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
        log::error("async_process: cannot run {}", argv.front());
        return false;
    }
    write_end = UniqueFd();

    uint64_t generation = 0;
    {
        std::lock_guard lock(shared_->mutex);
        shared_->pid = pid;
        shared_->done = false;
        shared_->output.clear();
        generation = ++shared_->generation;
    }
    done_ = std::move(done);
    running_ = true;

    std::thread([shared = shared_, fd = std::move(read_end), pid, generation] {
        std::string output;
        std::array<char, 4096> buf{};
        while (true) {
            ssize_t n = read(fd.get(), buf.data(), buf.size());
            if (n > 0) {
                output.append(buf.data(), static_cast<std::size_t>(n));
            } else if (n < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        waitpid(pid, nullptr, 0);
        std::lock_guard lock(shared->mutex);
        if (shared->generation != generation) {
            return;
        }
        shared->pid = -1;
        shared->output = std::move(output);
        shared->done = true;
        uint64_t one = 1;
        (void)!write(shared->wake.get(), &one, sizeof one);
    }).detach();
    return true;
}

void AsyncProcess::cancel() {
    {
        std::lock_guard lock(shared_->mutex);
        if (shared_->pid > 0) {
            kill(shared_->pid, SIGKILL);
            shared_->pid = -1;
        }
        ++shared_->generation;
        shared_->done = false;
        shared_->output.clear();
    }
    running_ = false;
    done_ = nullptr;
}

void AsyncProcess::finish() {
    uint64_t value = 0;
    while (read(shared_->wake.get(), &value, sizeof value) > 0) {
    }
    std::string output;
    {
        std::lock_guard lock(shared_->mutex);
        if (!shared_->done) {
            return;
        }
        shared_->done = false;
        output = std::move(shared_->output);
    }
    running_ = false;
    Done done = std::exchange(done_, nullptr);
    if (done) {
        done(std::move(output));
    }
}

} // namespace astralia
