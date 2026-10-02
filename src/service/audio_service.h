#pragma once

#include <memory>
#include <span>

#include "core/event_loop.h"
#include "core/signal.h"

namespace astralia {

enum class AudioKind { sink,
                       source };

struct AudioLevel {
    int percent = 0;
    bool muted = false;
    bool present = false;
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
    void set_sink_volume(int percent);

    Signal<AudioKind> changed;

  private:
    struct Impl;

    EventLoop &loop_;
    std::unique_ptr<Impl> impl_;
};

} // namespace astralia
