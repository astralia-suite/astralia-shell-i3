#include <algorithm>
#include <optional>
#include <set>

#include "core/log.h"

#include "service/media_service.h"

namespace astralia {

namespace {

const std::string daemon_service = "org.freedesktop.DBus";
const std::string daemon_path = "/org/freedesktop/DBus";
const std::string player_path = "/org/mpris/MediaPlayer2";
const std::string player_interface = "org.mpris.MediaPlayer2.Player";
const std::string properties_interface = "org.freedesktop.DBus.Properties";
const std::string name_prefix = "org.mpris.MediaPlayer2.";

using Metadata = std::map<std::string, sdbus::Variant>;

template <typename T>
std::optional<T> variant_get(const sdbus::Variant &value) {
    try {
        return value.get<T>();
    } catch (const sdbus::Error &) {
        return std::nullopt;
    }
}

template <typename T>
std::optional<T> metadata_get(const Metadata &metadata, const std::string &key) {
    auto it = metadata.find(key);
    return it == metadata.end() ? std::nullopt : variant_get<T>(it->second);
}

MediaTrack parse_metadata(const Metadata &metadata) {
    MediaTrack track;
    track.title = metadata_get<std::string>(metadata, "xesam:title").value_or("");
    if (auto artists = metadata_get<std::vector<std::string>>(metadata, "xesam:artist"); artists && !artists->empty()) {
        track.artist = artists->front();
    }
    track.art_url = metadata_get<std::string>(metadata, "mpris:artUrl").value_or("");
    track.length_us = metadata_get<int64_t>(metadata, "mpris:length").value_or(0);
    return track;
}

} // namespace

MediaPlayback media_parse_playback(const std::string &status) {
    if (status == "Playing") {
        return MediaPlayback::playing;
    }
    if (status == "Paused") {
        return MediaPlayback::paused;
    }
    return MediaPlayback::stopped;
}

std::string media_format_position(int64_t position_us) {
    int64_t seconds = std::max<int64_t>(position_us, 0) / 1000000;
    std::string result = std::to_string(seconds / 60) + ":";
    if (seconds % 60 < 10) {
        result += "0";
    }
    return result + std::to_string(seconds % 60);
}

int media_select_player(const std::vector<MediaPlayer> &players) {
    for (std::size_t i = 0; i < players.size(); ++i) {
        if (players[i].playback == MediaPlayback::playing) {
            return static_cast<int>(i);
        }
    }
    return players.empty() ? -1 : 0;
}

MediaService::MediaService(SystemBus &bus) : bus_(bus), daemon_(bus_.proxy(daemon_service, daemon_path)) {
    owner_match_ = bus_.add_match("type='signal',sender='org.freedesktop.DBus',member='NameOwnerChanged',"
                                  "arg0namespace='org.mpris.MediaPlayer2'",
                                  [this] { scan(); });
    properties_match_ = bus_.add_match("type='signal',interface='org.freedesktop.DBus.Properties',"
                                       "member='PropertiesChanged',path='/org/mpris/MediaPlayer2',"
                                       "arg0='org.mpris.MediaPlayer2.Player'",
                                       [this] { scan(); });
    scan();
}

sdbus::IProxy *MediaService::player(const std::string &name) {
    auto it = players_.find(name);
    if (it == players_.end()) {
        it = players_.emplace(name, bus_.proxy(name, player_path)).first;
    }
    return it->second.get();
}

void MediaService::scan() {
    if (!daemon_) {
        return;
    }
    uint64_t generation = ++generation_;
    try {
        daemon_->callMethodAsync("ListNames").onInterface(daemon_service).uponReplyInvoke([this, generation](std::optional<sdbus::Error> error, std::vector<std::string> names) {
            if (error || generation != generation_) {
                return;
            }
            std::set<std::string> live;
            for (const std::string &name : names) {
                if (name.starts_with(name_prefix)) {
                    live.insert(name);
                }
            }
            std::erase_if(players_, [&](const auto &entry) { return !live.contains(entry.first); });
            if (!live.contains(status_.bus_name)) {
                position_inflight_ = false;
            }
            candidates_.clear();
            expected_ = live.size();
            if (live.empty()) {
                select();
                return;
            }
            for (const std::string &name : live) {
                sdbus::IProxy *proxy = player(name);
                if (proxy == nullptr) {
                    collect(generation, name, MediaPlayback::stopped);
                    continue;
                }
                proxy->callMethodAsync("Get").onInterface(properties_interface).withArguments(player_interface, std::string("PlaybackStatus")).uponReplyInvoke([this, generation, name](std::optional<sdbus::Error> error, sdbus::Variant value) {
                    std::optional<std::string> status = error ? std::nullopt : variant_get<std::string>(value);
                    collect(generation, name, media_parse_playback(status.value_or("")));
                });
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("media: cannot list players: {}", error.what());
    }
}

void MediaService::collect(uint64_t generation, const std::string &name, MediaPlayback playback) {
    if (generation != generation_) {
        return;
    }
    candidates_.push_back({name, playback});
    if (candidates_.size() >= expected_) {
        select();
    }
}

void MediaService::select() {
    int selected = media_select_player(candidates_);
    if (selected < 0) {
        update(MediaStatus{});
        return;
    }
    const MediaPlayer &pick = candidates_[static_cast<std::size_t>(selected)];
    fetch_metadata(pick.bus_name, pick.playback);
}

void MediaService::fetch_metadata(const std::string &name, MediaPlayback playback) {
    sdbus::IProxy *proxy = player(name);
    if (proxy == nullptr) {
        return;
    }
    uint64_t generation = generation_;
    try {
        proxy->callMethodAsync("Get").onInterface(properties_interface).withArguments(player_interface, std::string("Metadata")).uponReplyInvoke([this, generation, name, playback](std::optional<sdbus::Error> error, sdbus::Variant value) {
            if (generation != generation_) {
                return;
            }
            MediaStatus next{true, name, playback, {}};
            if (!error) {
                if (auto metadata = variant_get<Metadata>(value)) {
                    next.track = parse_metadata(*metadata);
                }
            }
            if (status_.bus_name == name && status_.track.title == next.track.title) {
                next.track.position_us = status_.track.position_us;
            }
            update(next);
        });
    } catch (const sdbus::Error &error) {
        log::error("media: cannot read metadata: {}", error.what());
    }
}

void MediaService::update(const MediaStatus &next) {
    if (next == status_) {
        return;
    }
    status_ = next;
    changed.emit();
}

void MediaService::call(const char *method) {
    if (!status_.has_player) {
        return;
    }
    sdbus::IProxy *proxy = player(status_.bus_name);
    if (proxy == nullptr) {
        return;
    }
    try {
        proxy->callMethodAsync(method).onInterface(player_interface).uponReplyInvoke([method](std::optional<sdbus::Error> error) {
            if (error) {
                log::error("media: {} failed: {}", method, error->what());
            }
        });
    } catch (const sdbus::Error &error) {
        log::error("media: cannot call {}: {}", method, error.what());
    }
}

void MediaService::play_pause() { call("PlayPause"); }

void MediaService::next() { call("Next"); }

void MediaService::previous() { call("Previous"); }

void MediaService::poll_position() {
    if (!status_.has_player || position_inflight_) {
        return;
    }
    sdbus::IProxy *proxy = player(status_.bus_name);
    if (proxy == nullptr) {
        return;
    }
    std::string name = status_.bus_name;
    try {
        position_inflight_ = true;
        proxy->callMethodAsync("Get").onInterface(properties_interface).withArguments(player_interface, std::string("Position")).uponReplyInvoke([this, name](std::optional<sdbus::Error> error, sdbus::Variant value) {
            position_inflight_ = false;
            if (error || status_.bus_name != name) {
                return;
            }
            if (auto position = variant_get<int64_t>(value)) {
                MediaStatus next = status_;
                next.track.position_us = *position;
                update(next);
            }
        });
    } catch (const sdbus::Error &) {
        position_inflight_ = false;
    }
}

} // namespace astralia
