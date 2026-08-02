// Counts every trip to the heap in the test binary.
//
// Exists so that "no allocation on this path" can be a test rather than a
// comment. TI-027's decoder and TI-030's TextBuffer both make that claim, and
// a claim about allocation is exactly the kind that quietly stops being true.
//
// The counters are global because operator new is: an AllocationGuard reads
// the count when it is constructed and reports the delta, so nested or
// concurrent use would confuse it. Tests here are single-threaded.
#ifndef TYPEIT_TESTING_ALLOCATIONCOUNTER_H
#define TYPEIT_TESTING_ALLOCATIONCOUNTER_H

#include <cstddef>

namespace typeit::testing {

    /// Allocations since the process started.
    [[nodiscard]] std::size_t total_allocations() noexcept;

    class AllocationGuard {
    public:
        AllocationGuard() noexcept : start_{total_allocations()} {}

        [[nodiscard]] std::size_t count() const noexcept { return total_allocations() - start_; }

    private:
        std::size_t start_;
    };

}  // namespace typeit::testing

#endif  // TYPEIT_TESTING_ALLOCATIONCOUNTER_H
