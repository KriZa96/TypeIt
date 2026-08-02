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
// including the ones inside the standard library. Only the sized forms that
// route through malloc are defined; the array and aligned forms fall back to
// these or to the default, and free() matches malloc() either way.
//
// Over-aligned allocations go through the aligned overloads, which are left
// alone and therefore uncounted; nothing measured here is over-aligned.
//
// Under ASan this takes over from the sanitizer's own operator new, so
// new/delete mismatch detection is lost in the test binary while malloc-level
// checking is not. That trade is worth one counter.
void* operator new(std::size_t size) {
    ++typeit::testing::g_allocations;
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        throw std::bad_alloc{};
    }
    return memory;
}

void operator delete(void* memory) noexcept { std::free(memory); }

void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
