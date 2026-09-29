#pragma once

#include <unistd.h>
#include <utility>

namespace astralia {

class UniqueFd {
  public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : fd_(fd) {}
    UniqueFd(UniqueFd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    UniqueFd &operator=(UniqueFd &&other) noexcept {
        std::swap(fd_, other.fd_);
        return *this;
    }
    ~UniqueFd() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    int get() const { return fd_; }
    int release() { return std::exchange(fd_, -1); }

  private:
    int fd_ = -1;
};

} // namespace astralia
