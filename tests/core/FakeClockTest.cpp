#include <gtest/gtest.h>

#include "typeit/core/util/IClock.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"

namespace typeit::testing {
    namespace {

        using core::Millis;

        TEST(FakeClockTest, StartsWhereItIsToldTo) {
            EXPECT_EQ(FakeClock{}.now(), Millis{0});
            EXPECT_EQ(FakeClock{Millis{5'000}}.now(), Millis{5'000});
        }

        TEST(FakeClockTest, AdvanceAccumulates) {
            FakeClock clock;

            clock.advance(Millis{250});
            clock.advance(Millis{750});

            EXPECT_EQ(clock.now(), Millis{1'000});
        }

        TEST(FakeClockTest, SetReplaces) {
            FakeClock clock{Millis{9'000}};

            clock.set(Millis{42});

            EXPECT_EQ(clock.now(), Millis{42});
        }

        // The whole point. TimerTest currently sleeps for a second and then asserts
        // the string "9s", which is a race it happens to win; here a hundred reads
        // return the same reading because nothing moved time.
        TEST(FakeClockTest, TimeNeverMovesOnItsOwn) {
            const FakeClock clock{Millis{1'234}};

            for (int i = 0; i < 100; ++i) {
                EXPECT_EQ(clock.now(), Millis{1'234});
            }
        }

        TEST(FakeClockTest, ClocksAreIndependent) {
            FakeClock first;
            FakeClock second;

            first.advance(Millis{500});

            EXPECT_EQ(first.now(), Millis{500});
            EXPECT_EQ(second.now(), Millis{0});
        }

        TEST(FakeClockTest, GoesBackwardsWhenAskedTo) {
            // A clock that steps backwards is a thing that happens; code that reads
            // time needs to be testable against it.
            FakeClock clock{Millis{1'000}};

            clock.advance(Millis{-400});

            EXPECT_EQ(clock.now(), Millis{600});
        }

        TEST(FakeClockTest, SubstitutesForTheInterface) {
            FakeClock clock{Millis{7}};
            const core::IClock& port = clock;

            EXPECT_EQ(port.now(), Millis{7});

            clock.advance(Millis{3});
            EXPECT_EQ(port.now(), Millis{10});
        }

    }  // namespace
}  // namespace typeit::testing
