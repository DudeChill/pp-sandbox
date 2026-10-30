#pragma once

#include <cstdint>

namespace sim {

// Compact generational handle: 8 bytes, no allocation, stale-handle detection.
template <class Tag>
struct Handle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    [[nodiscard]] bool valid() const { return index != UINT32_MAX; }

    friend bool operator==(Handle a, Handle b) {
        return a.index == b.index && a.generation == b.generation;
    }
    friend bool operator!=(Handle a, Handle b) { return !(a == b); }
    friend bool operator<(Handle a, Handle b) {
        return a.index < b.index || (a.index == b.index && a.generation < b.generation);
    }
};

struct BodyTag {};
using BodyId = Handle<BodyTag>;

} // namespace sim
