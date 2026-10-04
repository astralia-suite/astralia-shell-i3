#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "render/image_decode.h"

namespace astralia::cover_cache_config {

// Cache location
inline constexpr const char *dir = "astralia-shell/covers";

// Pruning
inline constexpr uintmax_t max_bytes = 128ull * 1024 * 1024;
inline constexpr int64_t max_age_seconds = 90ll * 24 * 3600;
inline constexpr uintmax_t prune_after_bytes = 32ull * 1024 * 1024;
inline constexpr int64_t stale_temporary_seconds = 3600;

// Encoding
inline constexpr int jpeg_quality = 90;

} // namespace astralia::cover_cache_config

namespace astralia {

struct CoverCacheEntry {
    std::string name;
    uintmax_t size;
    int64_t used;
};

std::vector<std::string> cover_cache_expired(std::vector<CoverCacheEntry> entries, int64_t now);
std::string cover_cache_name(std::string_view path, uintmax_t size, int64_t modified, int width, int height);
std::expected<SurfacePtr, std::string> load_cover(const std::string &path, int width, int height);

} // namespace astralia
