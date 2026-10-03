#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

enum class AudioKind { sink,
                       source,
                       nodes };

struct AudioLevel {
    int percent = 0;
    bool muted = false;
    bool present = false;
};

enum class AudioNodeKind { sink,
                           source,
                           playback,
                           capture };

struct AudioNode {
    uint32_t id = 0;
    AudioNodeKind kind = AudioNodeKind::sink;
    std::string label;
    int percent = 0;
    bool muted = false;
};

int audio_percent(std::span<const float> channel_volumes);

class AudioService {
  public:
    explicit AudioService(EventLoop &loop);
    ~AudioService();
    AudioService(const AudioService &) = delete;
    AudioService &operator=(const AudioService &) = delete;

    AudioLevel sink() const;
    AudioLevel source() const;
    uint32_t sink_id() const;
    uint32_t source_id() const;
    std::vector<AudioNode> nodes(AudioNodeKind kind) const;
    std::optional<AudioNode> node(uint32_t id) const;
    void set_sink_volume(int percent);
    void set_sink_mute(bool muted);
    void set_volume(uint32_t id, int percent);
    void set_mute(uint32_t id, bool muted);
    void set_default(uint32_t id);

    Signal<AudioKind> changed;

  private:
    struct Impl;

    EventLoop &loop_;
    std::unique_ptr<Impl> impl_;
};

} // namespace astralia
