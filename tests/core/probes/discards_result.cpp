// This must NOT compile with warnings as errors: a discarded Result is an
// ignored failure, which is the whole reason ADR-009 returns errors as values
// instead of setting a flag somebody forgets to read.
#include "typeit/core/util/Result.h"

namespace {

    typeit::core::Result<int> might_fail() { return 1; }

    void ignores_the_answer() { might_fail(); }

}  // namespace
