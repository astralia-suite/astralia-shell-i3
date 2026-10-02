#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace astralia {

template <typename... Args>
class Signal {
  public:
    void connect(std::function<void(Args...)> slot) { slots_.push_back(std::move(slot)); }

    void emit(Args... args) const {
        for (const auto &slot : slots_) {
            slot(args...);
        }
    }

  private:
    std::vector<std::function<void(Args...)>> slots_;
};

} // namespace astralia
