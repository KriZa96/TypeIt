#include "typeit/core/text_supply/Prng.h"

#include <cstdint>
#include <random>

namespace typeit::core {

    std::uint64_t random_seed() {
        std::random_device entropy;
        // Two draws, because random_device yields 32 bits on every platform
        // that matters and a 32-bit seed is a small space to collide in.
        return (static_cast<std::uint64_t>(entropy()) << 32U) | entropy();
    }

}  // namespace typeit::core
