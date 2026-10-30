#include "alloc.h"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace sim {
namespace {
std::atomic<std::size_t> g_allocs{0};

void* aligned_alloc_impl(std::size_t n, std::size_t a) {
#if defined(_WIN32)
    return _aligned_malloc(n ? n : 1, a);
#else
    void* p = nullptr;
    if (posix_memalign(&p, a, n ? n : 1) != 0) return nullptr;
    return p;
#endif
}

void aligned_free_impl(void* p) {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
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
    void* p = sim::aligned_alloc_impl(n, a < sizeof(void*) ? sizeof(void*) : a);
    if (!p) throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t n, std::align_val_t al) { return ::operator new(n, al); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { sim::aligned_free_impl(p); }
void operator delete[](void* p, std::align_val_t) noexcept { sim::aligned_free_impl(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { sim::aligned_free_impl(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { sim::aligned_free_impl(p); }
