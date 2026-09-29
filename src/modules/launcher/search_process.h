#pragma once

#include <string>
#include <sys/types.h>
#include <vector>

#include "core/unique_fd.h"

namespace astralia {

class SearchProcess {
  public:
    SearchProcess() = default;
    ~SearchProcess();
    SearchProcess(const SearchProcess &) = delete;
    SearchProcess &operator=(const SearchProcess &) = delete;

    bool start(const std::vector<std::string> &argv);
    bool read_available();
    void cancel();
    bool running() const { return pid_ > 0; }
    int fd() const { return out_.get(); }
    const std::string &output() const { return buffer_; }

  private:
    void reap();

    pid_t pid_ = -1;
    UniqueFd out_;
    std::string buffer_;
};

} // namespace astralia
