#include <gtest/gtest.h>

// TI-023 ships no domain code, so this proves only what it can: the target
// exists, links, and runs. The interesting half of the issue is the isolation
// pair in CMakeLists.txt. TI-024 replaces this file with tests that assert
// something.
TEST(CoreScaffold, LinksAgainstTypeitCore) { SUCCEED(); }
