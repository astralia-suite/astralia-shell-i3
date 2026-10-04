#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <malloc.h>
#include <mutex>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "config/settings_config.h"

#include "core/log.h"
#include "core/unique_fd.h"

#include "modules/settings/thumbnail_cache.h"

#include "render/cover_cache.h"

namespace astralia {

namespace {

namespace cfg = settings_config;

constexpr int side = static_cast<int>(cfg::thumb_size);

SurfacePtr render_thumbnail(const std::string &path) {
    auto image = load_cover(path, side, side);
    if (!image) {
        log::error("settings: thumbnail {}: {}", path, image.error());
        return nullptr;
    }
    return std::move(*image);
}

} // namespace

struct ThumbnailCache::Shared {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::string> queue;
    std::string current;
    std::vector<std::pair<std::string, SurfacePtr>> done;
    bool stopped = false;
    UniqueFd event;
};

namespace {

void work(const std::shared_ptr<ThumbnailCache::Shared> &shared);

}

ThumbnailCache::ThumbnailCache(EventLoop &loop, std::function<void()> ready)
    : loop_(loop), ready_(std::move(ready)) {}

ThumbnailCache::~ThumbnailCache() { stop(); }

void ThumbnailCache::start() {
    if (shared_) {
        return;
    }
    shared_ = std::make_shared<Shared>();
    shared_->event = UniqueFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    loop_.on_fd(shared_->event.get(), [this] { collect(); });
    std::thread([shared = shared_] { work(shared); }).detach();
}

void ThumbnailCache::stop() {
    if (!shared_) {
        return;
    }
    loop_.remove_fd(shared_->event.get());
    {
        std::lock_guard lock(shared_->mutex);
        shared_->stopped = true;
        shared_->queue.clear();
    }
    shared_->wake.notify_all();
    shared_.reset();
    clear();
}

void ThumbnailCache::clear() {
    if (shared_) {
        std::lock_guard lock(shared_->mutex);
        shared_->queue.clear();
    }
    thumbnails_.clear();
    malloc_trim(0);
}

cairo_surface_t *ThumbnailCache::get(const std::string &path) const {
    auto it = thumbnails_.find(path);
    return it != thumbnails_.end() ? it->second.get() : nullptr;
}

void ThumbnailCache::request(const std::vector<std::string> &paths) {
    if (!shared_) {
        return;
    }
    {
        std::lock_guard lock(shared_->mutex);
        shared_->queue.clear();
        for (const std::string &path : paths) {
            if (!thumbnails_.contains(path) && path != shared_->current) {
                shared_->queue.push_back(path);
            }
        }
    }
    shared_->wake.notify_one();
}

void ThumbnailCache::collect() {
    if (!shared_) {
        return;
    }
    std::uint64_t counter = 0;
    if (read(shared_->event.get(), &counter, sizeof(counter)) < 0) {
        return;
    }
    std::vector<std::pair<std::string, SurfacePtr>> finished;
    {
        std::lock_guard lock(shared_->mutex);
        finished.swap(shared_->done);
    }
    for (auto &[path, thumbnail] : finished) {
        if (thumbnails_.size() >= cfg::thumb_cache_limit) {
            thumbnails_.clear();
        }
        thumbnails_.insert_or_assign(std::move(path), std::move(thumbnail));
    }
    if (!finished.empty()) {
        ready_();
    }
}

namespace {

void work(const std::shared_ptr<ThumbnailCache::Shared> &shared) {
    for (;;) {
        std::string path;
        {
            std::unique_lock lock(shared->mutex);
            shared->wake.wait(lock, [&] { return shared->stopped || !shared->queue.empty(); });
            if (shared->stopped) {
                return;
            }
            path = std::move(shared->queue.front());
            shared->queue.pop_front();
            shared->current = path;
        }
        SurfacePtr thumbnail = render_thumbnail(path);
        {
            std::lock_guard lock(shared->mutex);
            shared->current.clear();
            if (shared->stopped) {
                return;
            }
            shared->done.emplace_back(std::move(path), std::move(thumbnail));
        }
        std::uint64_t one = 1;
        if (write(shared->event.get(), &one, sizeof(one)) < 0) {
            return;
        }
    }
}

} // namespace

} // namespace astralia
