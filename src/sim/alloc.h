#pragma once

#include <cstddef>

namespace sim {

std::size_t allocation_count() noexcept;
void reset_allocation_count() noexcept;

// RAII helper for tests: counts allocations performed inside the scope.
struct AllocationGuard {
    std::size_t before = 0;
    AllocationGuard() : before(allocation_count()) {}
    [[nodiscard]] std::size_t allocations() const {
        return allocation_count() - before;
    }
};

} // namespace sim
