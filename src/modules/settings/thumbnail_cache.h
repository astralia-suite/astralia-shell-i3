#pragma once

#include <cairo.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/event_loop.h"

#include "render/image_decode.h"

namespace astralia {

class ThumbnailCache {
  public:
    struct Shared;

    ThumbnailCache(EventLoop &loop, std::function<void()> ready);
    ~ThumbnailCache();
    ThumbnailCache(const ThumbnailCache &) = delete;
    ThumbnailCache &operator=(const ThumbnailCache &) = delete;

    void start();
    void stop();
    void clear();
    cairo_surface_t *get(const std::string &path) const;
    bool contains(const std::string &path) const { return thumbnails_.contains(path); }
    void request(const std::vector<std::string> &paths);

  private:
    void collect();

    EventLoop &loop_;
    std::function<void()> ready_;
    std::shared_ptr<Shared> shared_;
    std::unordered_map<std::string, SurfacePtr> thumbnails_;
};

} // namespace astralia
