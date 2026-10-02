#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <sys/inotify.h>
#include <unistd.h>

#include "core/log.h"
#include "core/spawn.h"

#include "service/brightness_service.h"

namespace astralia {

namespace {

constexpr const char *backlight_dir = "/sys/class/backlight";

int read_int(const std::filesystem::path &path) {
    std::ifstream file(path);
    int value = 0;
    file >> value;
    return value;
}

} // namespace

BrightnessService::BrightnessService(EventLoop &loop) : loop_(loop) {
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(backlight_dir, error)) {
        device_ = entry.path().filename();
        max_ = read_int(entry.path() / "max_brightness");
        break;
    }
    if (!available()) {
        log::info("brightness: no backlight device");
        return;
    }
    std::filesystem::path file = std::filesystem::path(backlight_dir) / device_ / "brightness";
    watch_ = UniqueFd(inotify_init1(IN_NONBLOCK | IN_CLOEXEC));
    if (watch_.get() < 0 || inotify_add_watch(watch_.get(), file.c_str(), IN_MODIFY) < 0) {
        log::error("brightness: cannot watch {}", file.string());
        watch_ = UniqueFd();
        return;
    }
    loop_.on_fd(watch_.get(), [this] {
        std::array<char, 256> buffer{};
        bool modified = false;
        while (read(watch_.get(), buffer.data(), buffer.size()) > 0) {
            modified = true;
        }
        if (modified) {
            changed.emit();
        }
    });
}

BrightnessService::~BrightnessService() {
    if (watch_.get() >= 0) {
        loop_.remove_fd(watch_.get());
    }
}

int BrightnessService::percent() const {
    if (!available()) {
        return 0;
    }
    int current = read_int(std::filesystem::path(backlight_dir) / device_ / "brightness");
    return static_cast<int>(std::lround(current * 100.0 / max_));
}

void BrightnessService::set(int percent) const {
    if (available()) {
        spawn_detached(std::format("brightnessctl -q -d {} set {}%", device_, percent));
    }
}

} // namespace astralia
