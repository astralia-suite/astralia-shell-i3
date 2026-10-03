#include "app/services.h"

namespace astralia {

Services::Services(XConnection &x, EventLoop &loop)
    : system(loop), session(loop, BusKind::session), i3(x, loop), network(system, loop),
      bluetooth(system), battery(system), brightness(loop), notifications(loop), polkit(loop),
      audio(loop), media(session) {}

} // namespace astralia
