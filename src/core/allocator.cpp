#include <malloc.h>

#include "core/allocator.h"

namespace astralia {

namespace {

constexpr int mmap_threshold = 1 << 20;

} // namespace

void tune_allocator() {
    mallopt(M_ARENA_MAX, 1);
    mallopt(M_MMAP_THRESHOLD, mmap_threshold);
}

} // namespace astralia
