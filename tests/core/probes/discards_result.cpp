// This must NOT compile with warnings as errors: a discarded Result is an
// ignored failure, which is the whole reason ADR-009 returns errors as values
// instead of setting a flag somebody forgets to read.
//
// The [[nodiscard]] is on the function, not inherited from std::expected --
// half the standard libraries this project builds against do not mark the
// type, and a guarantee that holds on some of them is not a guarantee.
#include "typeit/core/util/Result.h"

namespace {

    [[nodiscard]] typeit::core::Result<int> might_fail() { return 1; }

    void ignores_the_answer() { might_fail(); }

}  // namespace
