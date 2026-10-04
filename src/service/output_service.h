#pragma once

#include <vector>

#include "core/event_loop.h"
#include "core/signal.h"
#include "core/x_connection.h"

namespace astralia {

class OutputService {
  public:
    OutputService(XConnection &x, EventLoop &loop);

    const std::vector<Output> &outputs() const { return outputs_; }
    const Output &at_pointer() const;

    Signal<> changed;

  private:
    void refresh();

    XConnection &x_;
    std::vector<Output> outputs_;
};

} // namespace astralia
