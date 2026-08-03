#include "typeit/testing/AllocationCounter.h"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace typeit::testing {
    namespace {

        std::size_t g_allocations = 0;

    }  // namespace

    std::size_t total_allocations() noexcept { return g_allocations; }

}  // namespace typeit::testing

// Replacing global operator new is the only way to see every allocation,
// including the ones inside the standard library.
//
// Every non-aligned form is replaced, not just the one the tests obviously
// need. A partial replacement is worse than none: the standard library is free
// to pair any new with any delete, and libc++'s temporary buffer does exactly
// that — it allocates with the nothrow form and frees with the plain one. With
// only the plain form replaced, that pair straddles two allocators, and ASan
// reports the alloc-dealloc mismatch it correctly is.
//
// Over-aligned allocations are the one exception: they are left to the default
// entirely, both halves, so the pair still matches. `aligned_alloc` and
// `_aligned_malloc` do not free the same way on every platform, and nothing
// measured here is over-aligned.
//
// Under ASan this takes over from the sanitizer's own operator new, so
// new/delete mismatch detection is lost in the test binary while malloc-level
// checking is not. That trade is worth one counter.
namespace {

    void* counted_malloc(std::size_t size) noexcept {
        ++typeit::testing::g_allocations;
        return std::malloc(size == 0 ? 1 : size);
    }

    void* counted_malloc_or_throw(std::size_t size) {
        void* memory = counted_malloc(size);
        if (memory == nullptr) {
            throw std::bad_alloc{};
        }
        return memory;
    }

}  // namespace

void* operator new(std::size_t size) { return counted_malloc_or_throw(size); }
void* operator new[](std::size_t size) { return counted_malloc_or_throw(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return counted_malloc(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return counted_malloc(size); }

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
