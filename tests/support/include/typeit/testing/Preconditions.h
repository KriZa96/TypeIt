// Testing a precondition, in a build that has preconditions.
//
// This project states its contracts with `assert` (ADR-009): a caller that
// decrements a `GraphemeIndex` past zero, or asks for a random number below
// zero of them, has a bug, and the assert is what says so. `NDEBUG` removes
// every one of them, which is the point of a release build.
//
// `EXPECT_DEBUG_DEATH` is the obvious tool and is a trap here. Under `NDEBUG`
// it does not skip the case: it **executes the statement** and expects it to
// survive. So a release build runs every contract violation the suite knows
// about, for real, with the checks compiled out.
//
// Mostly that is invisible — reading one past the end of a buffer returns
// whatever was next to it. `Prng::below(0)` is the one that is not: it computes
// a remainder modulo the bound, so a bound of zero is an integer division by
// zero, which traps. It took the first Windows release job that got far enough
// to run its tests to find that, and every one of these cases was doing the
// same class of thing.
//
// So: where there are no preconditions, there is nothing to test.
#ifndef TYPEIT_TESTING_PRECONDITIONS_H
#define TYPEIT_TESTING_PRECONDITIONS_H

#include <gtest/gtest.h>

#ifdef NDEBUG

/// Skips, rather than running the violation with the check removed.
///
/// The statement is still compiled — inside a branch that is never taken — so
/// that it goes on being type-checked and its locals go on being used. A case
/// that quietly stopped compiling in one configuration would be a case nobody
/// noticed had rotted.
#define TYPEIT_EXPECT_PRECONDITION(statement, matcher)                          \
    do {                                                                        \
        if (false) {                                                            \
            statement;                                                          \
            static_cast<void>(matcher);                                         \
        }                                                                       \
        GTEST_SKIP() << "a precondition is an assert, and this build has none"; \
    } while (false)

#else

#define TYPEIT_EXPECT_PRECONDITION(statement, matcher) EXPECT_DEBUG_DEATH(statement, matcher)

#endif

#endif  // TYPEIT_TESTING_PRECONDITIONS_H
