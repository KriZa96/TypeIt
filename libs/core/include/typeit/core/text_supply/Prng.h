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
        ///
        /// **Not** `std::uniform_int_distribution`. `std::mt19937_64` is
        /// specified exactly, so `next()` is the same everywhere; the
        /// *distribution* is not — how it maps engine output onto a range is
        /// implementation-defined, and libstdc++ and libc++ disagree. A run
        /// recorded on Linux and replayed on Windows would produce different
        /// text from the same seed, which makes "reproduce this run exactly"
        /// untrue on precisely the machine somebody is reading a bug report on.
        ///
        /// Rejection sampling instead, written out: draw 64 bits, throw away
        /// the values that would make the modulo biased, and take the
        /// remainder. `threshold` is 2^64 mod bound, computed as `-bound mod
        /// bound` in unsigned arithmetic — the values below it are the ones
        /// belonging to the short final block.
        [[nodiscard]] std::size_t below(std::size_t bound) {
            assert(bound > 0 && "no number is below zero of them");
            const auto span = static_cast<std::uint64_t>(bound);
            const std::uint64_t threshold = (~span + 1U) % span;

            std::uint64_t drawn = engine_();
            while (drawn < threshold) {
                drawn = engine_();
            }
            // `drawn % span` is already below `bound`, so it fits whatever
            // `size_t` is. No cast: on a 64-bit `size_t` the compiler calls one
            // useless and refuses to build.
            return drawn % span;
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
