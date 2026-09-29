#pragma once

#include <expected>
#include <string>
#include <utility>

#include "core/unique_fd.h"

namespace astralia {

class SingleInstance {
  public:
    static std::expected<SingleInstance, std::string> acquire(const std::string &path);

  private:
    explicit SingleInstance(UniqueFd fd) : fd_(std::move(fd)) {}

    UniqueFd fd_;
};

} // namespace astralia
