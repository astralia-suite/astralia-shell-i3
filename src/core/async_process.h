#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>

#include "core/event_loop.h"
#include "core/unique_fd.h"

namespace astralia {

class AsyncProcess {
  public:
    using Done = std::function<void(std::string output)>;

    explicit AsyncProcess(EventLoop &loop);
    ~AsyncProcess();
    AsyncProcess(const AsyncProcess &) = delete;
    AsyncProcess &operator=(const AsyncProcess &) = delete;

    bool start(const std::vector<std::string> &argv, Done done, bool merge_stderr = false);
    void cancel();
    bool running() const { return running_; }

  private:
    struct Shared {
        std::mutex mutex;
        UniqueFd wake;
        uint64_t generation = 0;
        pid_t pid = -1;
        bool done = false;
        std::string output;
    };

    void finish();

    EventLoop &loop_;
    std::shared_ptr<Shared> shared_;
    Done done_;
    bool running_ = false;
};

} // namespace astralia
