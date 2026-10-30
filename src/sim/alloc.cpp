#include "alloc.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace sim {
namespace {
std::atomic<std::size_t> g_allocs{0};
}

std::size_t allocation_count() noexcept { return g_allocs.load(std::memory_order_relaxed); }
void reset_allocation_count() noexcept { g_allocs.store(0, std::memory_order_relaxed); }

} // namespace sim

void* operator new(std::size_t n) {
    sim::g_allocs.fetch_add(1, std::memory_order_relaxed);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t n) { return ::operator new(n); }

void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    sim::g_allocs.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(n ? n : 1);
}

void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept {
    return ::operator new(n, tag);
}

void* operator new(std::size_t n, std::align_val_t al) {
    sim::g_allocs.fetch_add(1, std::memory_order_relaxed);
    const std::size_t a = static_cast<std::size_t>(al);
    void* p = nullptr;
    if (posix_memalign(&p, a < sizeof(void*) ? sizeof(void*) : a, n ? n : 1) != 0) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t n, std::align_val_t al) { return ::operator new(n, al); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
