// The domain's only source of randomness (GAMEPLAY section 5.4).
//
// Every provider that shuffles, samples or generates draws from one of these,
// and the seed is recorded with the run. "Reproduce this run exactly" is then a
// matter of one number — for a replay, for a bug report, and for a test that
// wants a random-looking stream without being at the mercy of one.
#ifndef TYPEIT_CORE_TEXT_SUPPLY_PRNG_H
#define TYPEIT_CORE_TEXT_SUPPLY_PRNG_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>

namespace typeit::core {

    class Prng {
    public:
        explicit Prng(std::uint64_t seed) noexcept : engine_{seed}, seed_{seed} {}

        [[nodiscard]] std::uint64_t next() noexcept { return engine_(); }

        /// A value in `[0, bound)`. Precondition: `bound > 0`.
        [[nodiscard]] std::size_t below(std::size_t bound) {
            assert(bound > 0 && "no number is below zero of them");
            return std::uniform_int_distribution<std::size_t>{0, bound - 1}(engine_);
        }

        /// The number this generator was built from, to be stored with the run.
        [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }

    private:
        std::mt19937_64 engine_;
        std::uint64_t seed_;
    };

    /// A seed for a run nobody asked to reproduce. Drawn from the platform's
    /// entropy source, and then recorded like any other — the point is that
    /// after the fact there is no such thing as an unseeded run.
    [[nodiscard]] std::uint64_t random_seed();

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SUPPLY_PRNG_H
