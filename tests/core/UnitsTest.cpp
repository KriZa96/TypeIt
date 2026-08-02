#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        // The compile-time half of ADR-010. These are the assertions that matter: a
        // runtime test cannot observe an illegal conversion, because the point is that
        // it never compiles.
        static_assert(!std::is_convertible_v<double, Wpm>, "Wpm w = 5.0; must not compile");
        static_assert(!std::is_convertible_v<double, Accuracy>);
        static_assert(!std::is_convertible_v<std::int64_t, Millis>);
        static_assert(!std::is_convertible_v<std::size_t, GraphemeIndex>);
        static_assert(!std::is_convertible_v<std::int64_t, SessionId>);
        static_assert(!std::is_convertible_v<std::int64_t, TextId>);

        static_assert(!std::is_constructible_v<Wpm, Accuracy>, "the six types must not interconvert");
        static_assert(!std::is_constructible_v<Accuracy, Wpm>);
        static_assert(!std::is_constructible_v<SessionId, TextId>);
        static_assert(!std::is_constructible_v<TextId, SessionId>);
        static_assert(!std::is_constructible_v<GraphemeIndex, Millis>);

        // Aggregate initialisation stays available; it is only the implicit form that
        // is refused.
        static_assert(std::is_constructible_v<Wpm, double>);
        static_assert(std::is_aggregate_v<Millis>);

        TEST(UnitsTest, ComparisonIsTotalWithinAType) {
            constexpr Wpm slow{40.0};
            constexpr Wpm fast{80.0};

            EXPECT_LT(slow, fast);
            EXPECT_GT(fast, slow);
            EXPECT_EQ(slow, Wpm{40.0});
            EXPECT_NE(slow, fast);
            EXPECT_LE(slow, Wpm{40.0});
            EXPECT_GE(fast, Wpm{80.0});
        }

        // A concept rather than an inline requires-expression: a non-dependent
        // requires-expression over a missing operator is a hard error on gcc.
        template<typename T>
        concept Addable = requires(T a, T b) { a + b; };

        static_assert(!Addable<Accuracy>, "the mean of two accuracies is not an accuracy");
        static_assert(!Addable<Wpm>);
        static_assert(!Addable<SessionId>);
        static_assert(Addable<Millis>, "durations do add");

        TEST(UnitsTest, AccuracyCompares) {
            EXPECT_LT(Accuracy{0.5}, Accuracy{0.95});
            EXPECT_EQ(Accuracy{1.0}, Accuracy{1.0});
        }

        TEST(UnitsTest, MillisSubtractsToMillis) {
            constexpr Millis start{1'000};
            constexpr Millis end{2'500};

            static_assert(std::is_same_v<decltype(end - start), Millis>);
            EXPECT_EQ(end - start, Millis{1'500});
            EXPECT_EQ(start + Millis{500}, Millis{1'500});
        }

        TEST(UnitsTest, MillisOrdersCorrectlyAcrossNegativeDifferences) {
            constexpr Millis earlier{500};
            constexpr Millis later{1'500};

            EXPECT_EQ(earlier - later, Millis{-1'000});
            EXPECT_LT(earlier - later, Millis{0});
            EXPECT_LT(Millis{-2'000}, Millis{-1'000});
            EXPECT_EQ(-Millis{750}, Millis{-750});
        }

        TEST(UnitsTest, MillisCompoundAssignment) {
            Millis elapsed{0};

            elapsed += Millis{250};
            elapsed += Millis{250};
            EXPECT_EQ(elapsed, Millis{500});

            elapsed -= Millis{600};
            EXPECT_EQ(elapsed, Millis{-100});
        }

        TEST(UnitsTest, GraphemeIndexIncrements) {
            GraphemeIndex cursor{0};

            EXPECT_EQ(++cursor, GraphemeIndex{1});
            EXPECT_EQ(cursor++, GraphemeIndex{1});
            EXPECT_EQ(cursor, GraphemeIndex{2});
            EXPECT_EQ(--cursor, GraphemeIndex{1});
            EXPECT_EQ(cursor--, GraphemeIndex{1});
            EXPECT_EQ(cursor, GraphemeIndex{0});
        }

        // Defect C7 in one line: on the byte-oriented model an empty text made
        // `size() - 1` SIZE_MAX, and only an unrelated early return kept the write in
        // bounds. Here it is a contract violation that fires in a debug build.
        TEST(UnitsTestDeath, GraphemeIndexRefusesToDecrementBelowZero) {
            GraphemeIndex cursor{0};

            EXPECT_DEBUG_DEATH(--cursor, "decremented below zero");
        }

        TEST(UnitsTest, IdentitiesAreDistinctTypesWithTheSamePayload) {
            constexpr SessionId session{7};
            constexpr TextId text{7};

            EXPECT_EQ(session.value, text.value);
            EXPECT_EQ(session, SessionId{7});
            EXPECT_LT(SessionId{6}, session);
        }

    }  // namespace
}  // namespace typeit::core
