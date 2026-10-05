#include <cstdio>
#include <pwd.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "service/user_service.h"

namespace astralia {

UserService::UserService() : name_("unknown") {
    const passwd *entry = getpwuid(getuid());
    if (entry == nullptr) {
        return;
    }
    if (entry->pw_gecos != nullptr && entry->pw_gecos[0] != '\0') {
        std::string full = entry->pw_gecos;
        std::string first = full.substr(0, full.find(','));
        if (!first.empty()) {
            name_ = first;
            return;
        }
    }
    if (entry->pw_name != nullptr) {
        name_ = entry->pw_name;
    }
}

std::string UserService::uptime() const {
    struct sysinfo info{};
    if (sysinfo(&info) != 0) {
        return {};
    }
    char text[32];
    std::snprintf(text, sizeof(text), "Up: %02ld:%02ld", info.uptime / 3600, (info.uptime % 3600) / 60);
    return text;
}

} // namespace astralia
