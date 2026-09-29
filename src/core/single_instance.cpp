#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>

#include "core/log.h"
#include "core/single_instance.h"

namespace astralia {

std::expected<SingleInstance, std::string> SingleInstance::acquire(const std::string &path) {
    UniqueFd fd(open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600));
    if (fd.get() < 0) {
        log::error("open {}: {}; running unguarded", path, std::strerror(errno));
        return SingleInstance(std::move(fd));
    }
    if (flock(fd.get(), LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK) {
            return std::unexpected("already running");
        }
        log::error("flock {}: {}; running unguarded", path, std::strerror(errno));
    }
    return SingleInstance(std::move(fd));
}

} // namespace astralia
