#include "app/services.h"

namespace astralia {

Services::Services(XConnection &x, EventLoop &loop)
    : system(loop), session(loop, BusKind::session), i3(x, loop), network(system),
      bluetooth(system), battery(system), brightness(loop), notifications(loop), polkit(loop),
      audio(loop) {}

} // namespace astralia
