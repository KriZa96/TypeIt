#include "typeit/core/text_supply/WholeTextProvider.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace typeit::core {

    WholeTextProvider::WholeTextProvider(std::string_view text, std::uint64_t seed) :
        text_{text}, seed_{seed}, exhausted_{text.empty()} {}

    std::string WholeTextProvider::next_chunk() {
        if (exhausted_) {
            return {};
        }
        exhausted_ = true;
        return text_;
    }

}  // namespace typeit::core
