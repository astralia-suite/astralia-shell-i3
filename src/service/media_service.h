#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

#include "core/dbus.h"
#include "core/signal.h"

namespace astralia {

enum class MediaPlayback { stopped,
                           paused,
                           playing };

struct MediaTrack {
    std::string title;
    std::string artist;
    std::string art_url;
    int64_t length_us = 0;
    int64_t position_us = 0;

    bool operator==(const MediaTrack &) const = default;
};

struct MediaStatus {
    bool has_player = false;
    std::string bus_name;
    MediaPlayback playback = MediaPlayback::stopped;
    MediaTrack track;

    bool operator==(const MediaStatus &) const = default;
};

struct MediaPlayer {
    std::string bus_name;
    MediaPlayback playback;
};

MediaPlayback media_parse_playback(const std::string &status);
std::string media_format_position(int64_t position_us);
int media_select_player(const std::vector<MediaPlayer> &players);

class MediaService {
  public:
    explicit MediaService(SystemBus &bus);

    const MediaStatus &status() const { return status_; }
    void play_pause();
    void next();
    void previous();
    void poll_position();

    Signal<> changed;

  private:
    void scan();
    void collect(uint64_t generation, const std::string &name, MediaPlayback playback);
    void select();
    void fetch_metadata(const std::string &name, MediaPlayback playback);
    void update(const MediaStatus &next);
    void call(const char *method);
    sdbus::IProxy *player(const std::string &name);

    SystemBus &bus_;
    std::unique_ptr<sdbus::IProxy> daemon_;
    std::map<std::string, std::unique_ptr<sdbus::IProxy>> players_;
    std::vector<MediaPlayer> candidates_;
    std::size_t expected_ = 0;
    uint64_t generation_ = 0;
    bool position_inflight_ = false;
    MediaStatus status_;
    sdbus::Slot owner_match_;
    sdbus::Slot properties_match_;
};

} // namespace astralia
