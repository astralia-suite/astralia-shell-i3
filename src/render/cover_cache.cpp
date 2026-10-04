#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <mutex>
#include <system_error>
#include <thread>

#include "render/cover_cache.h"

namespace astralia {

namespace {

std::string cache_directory() {
    const char *cache_home = std::getenv("XDG_CACHE_HOME");
    const char *home = std::getenv("HOME");
    std::string base = cache_home != nullptr && cache_home[0] != '\0' ? std::string(cache_home) : std::format("{}/.cache", home != nullptr ? home : "");
    return std::format("{}/{}", base, cover_cache_config::dir);
}

int64_t file_seconds(std::filesystem::file_time_type time) {
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

void prune(const std::filesystem::path &directory) {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    std::vector<CoverCacheEntry> entries;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        std::error_code entry_error;
        if (!entry.is_regular_file(entry_error)) {
            continue;
        }
        entries.push_back({entry.path().filename().string(), entry.file_size(entry_error), file_seconds(entry.last_write_time(entry_error))});
    }
    for (const std::string &name : cover_cache_expired(std::move(entries), file_seconds(std::filesystem::file_time_type::clock::now()))) {
        std::filesystem::remove(directory / name, error);
    }
}

void store(cairo_surface_t *surface, const std::filesystem::path &target) {
    static std::atomic<uintmax_t> pending{cover_cache_config::prune_after_bytes};
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    std::filesystem::path temporary = target;
    temporary += std::format(".{:x}.tmp", std::hash<std::thread::id>{}(std::this_thread::get_id()));
    if (cairo_surface_write_to_png(surface, temporary.c_str()) == CAIRO_STATUS_SUCCESS) {
        std::filesystem::rename(temporary, target, error);
    }
    if (error) {
        std::filesystem::remove(temporary, error);
        return;
    }
    uintmax_t size = std::filesystem::file_size(target, error);
    uintmax_t written = error ? 0 : size;
    if (pending.fetch_add(written) + written >= cover_cache_config::prune_after_bytes) {
        pending = 0;
        prune(target.parent_path());
    }
}

} // namespace

std::vector<std::string> cover_cache_expired(std::vector<CoverCacheEntry> entries, int64_t now) {
    std::vector<std::string> expired;
    std::vector<CoverCacheEntry> kept;
    uintmax_t total = 0;
    for (CoverCacheEntry &entry : entries) {
        int64_t age = now - entry.used;
        bool temporary = entry.name.ends_with(".tmp");
        if (age > (temporary ? cover_cache_config::stale_temporary_seconds : cover_cache_config::max_age_seconds)) {
            expired.push_back(entry.name);
        } else if (!temporary) {
            total += entry.size;
            kept.push_back(std::move(entry));
        }
    }
    std::ranges::sort(kept, {}, &CoverCacheEntry::used);
    for (const CoverCacheEntry &entry : kept) {
        if (total <= cover_cache_config::max_bytes) {
            break;
        }
        total -= entry.size;
        expired.push_back(entry.name);
    }
    return expired;
}

std::string cover_cache_name(std::string_view path, uintmax_t size, int64_t modified, int width, int height) {
    std::size_t key = std::hash<std::string>{}(std::format("{}|{}|{}|{}x{}", path, size, modified, width, height));
    return std::format("{:016x}.png", key);
}

std::expected<SurfacePtr, std::string> load_cover(const std::string &path, int width, int height) {
    std::error_code error;
    uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        return std::unexpected(error.message());
    }
    int64_t modified = std::filesystem::last_write_time(path, error).time_since_epoch().count();
    std::filesystem::path cached = std::filesystem::path(cache_directory()) / cover_cache_name(path, size, modified, width, height);
    if (auto stored = decode_image(cached.string())) {
        cairo_surface_t *surface = stored->get();
        if (cairo_image_surface_get_width(surface) == width && cairo_image_surface_get_height(surface) == height) {
            std::filesystem::last_write_time(cached, std::filesystem::file_time_type::clock::now(), error);
            return stored;
        }
    }
    auto image = decode_cover(path, width, height);
    if (image) {
        store(image->get(), cached);
    }
    return image;
}

} // namespace astralia
