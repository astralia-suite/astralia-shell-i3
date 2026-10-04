#include <algorithm>
#include <map>
#include <vector>

#include "core/log.h"

#include "modules/bar.h"
#include "modules/bar_set.h"

namespace astralia {

BarSet::BarSet(XConnection &x, EventLoop &loop, IpcServer &ipc, Services &services)
    : x_(x), loop_(loop), ipc_(ipc), services_(services) {
    notifier_ =
        services_.session.proxy("org.freedesktop.Notifications", "/org/freedesktop/Notifications");
    services_.bluetooth.messages.connect(
        [this](const StatusMessage &message) { notify("Bluetooth", message); });
    services_.network.messages.connect(
        [this](const StatusMessage &message) { notify("Network", message); });
    services_.outputs.changed.connect([this] { sync(); });
    services_.settings.changed.connect([this] { sync(); });
    sync();
}

BarSet::~BarSet() = default;

void BarSet::sync() {
    std::vector<std::string> shown;
    for (const Output &output : services_.outputs.outputs()) {
        if (!services_.settings.enabled(Feature::bar, output.name)) {
            continue;
        }
        shown.push_back(output.name);
        auto it = std::ranges::find(bars_, output.name, &Entry::output);
        if (it == bars_.end()) {
            bars_.push_back({output.name, std::make_unique<Bar>(x_, loop_, ipc_, services_, output)});
            continue;
        }
        it->bar->place(output);
    }
    for (Entry &entry : bars_) {
        if (std::ranges::find(shown, entry.output) == shown.end()) {
            entry.bar->hide();
        }
    }
}

void BarSet::notify(const std::string &app, const StatusMessage &message) {
    if (!notifier_) {
        return;
    }
    try {
        notifier_->callMethod("Notify")
            .onInterface("org.freedesktop.Notifications")
            .withArguments(app, uint32_t{0}, std::string(), message.summary, message.body,
                           std::vector<std::string>{}, std::map<std::string, sdbus::Variant>{},
                           int32_t{-1})
            .dontExpectReply();
    } catch (const sdbus::Error &error) {
        log::error("bar: cannot send notification: {}", error.what());
    }
}

} // namespace astralia
