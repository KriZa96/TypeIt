#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/metrics/ErrorMap.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        ErrorMap errors_of(const KeystrokeLog& log, std::string_view text) {
            return error_map(log, testing::text_of(text));
        }

        std::pair<std::string, std::string> pair_of(std::string_view expected, std::string_view typed) {
            return {std::string{expected}, std::string{typed}};
        }

        TEST(ErrorMapTest, ASubstitutionIsCountedOncePerFirstAttempt) {
            // Typed wrong, deleted, typed wrong the same way again, then fixed.
            // One mistake made repeatedly at one place is one mistake.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("x", Millis{0})
                                             .backspace(Millis{100})
                                             .type("x", Millis{200})
                                             .backspace(Millis{300})
                                             .type("a", Millis{400})
                                             .build();
            const ErrorMap errors = errors_of(log, "a");

            ASSERT_EQ(errors.substitutions.size(), 1U);
            EXPECT_EQ(errors.substitutions.at(pair_of("a", "x")), 1U);
        }

        TEST(ErrorMapTest, ACorrectedErrorStillAppears) {
            // You made it, even though you fixed it — and knowing that is the
            // whole point of the map.
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{0})
                                             .type("n", Millis{100})
                                             .backspace(Millis{200})
                                             .type("m", Millis{300})
                                             .build();
            const ErrorMap errors = errors_of(log, "am");

            EXPECT_EQ(errors.substitutions.at(pair_of("m", "n")), 1U);
        }

        TEST(ErrorMapTest, TheSameConfusionAtDifferentPlacesCountsEachTime) {
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("n", Millis{0}).type("a", Millis{100}).type("n", Millis{200}).build();
            const ErrorMap errors = errors_of(log, "mam");

            EXPECT_EQ(errors.substitutions.at(pair_of("m", "n")), 2U);
        }

        /// Appends one typed grapheme aimed at an explicit position — the
        /// shape a space-skip leaves in the log, which the sequential builder
        /// cannot express.
        void type_at(KeystrokeLog& log, std::uint32_t target, std::string_view grapheme, std::int64_t at) {
            const TextBuffer typed = testing::text_of(grapheme);
            log.append(Keystroke{.at = Millis{at},
                                 .target = target,
                                 .kind = KeystrokeKind::Character,
                                 .typed = typed.at(GraphemeIndex{0})});
        }

        TEST(ErrorMapTest, ASkippedPositionIsAnOmissionRatherThanASubstitution) {
            // Position 0 typed, 1 and 2 jumped over by a space, 3 typed.
            KeystrokeLog log;
            type_at(log, 0, "a", 0);
            type_at(log, 3, "d", 100);

            const ErrorMap errors = errors_of(log, "abcd");

            EXPECT_EQ(errors.omissions.at("b"), 1U);
            EXPECT_EQ(errors.omissions.at("c"), 1U);
            EXPECT_TRUE(errors.substitutions.empty());
            EXPECT_TRUE(errors.insertions.empty());
        }

        TEST(ErrorMapTest, APositionTypedAndThenEmptiedIsNotAnOmission) {
            // It was attempted. What it was attempted with is what the map
            // records, whatever happened to it afterwards.
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("x", Millis{100}).backspace(Millis{200}).build();
            const ErrorMap errors = errors_of(log, "abcd");

            EXPECT_TRUE(errors.omissions.empty());
            EXPECT_EQ(errors.substitutions.at(pair_of("b", "x")), 1U);
        }

        TEST(ErrorMapTest, TypingPastTheEndIsAnInsertion) {
            const KeystrokeLog log =
                    testing::LogBuilder{}.type("a", Millis{0}).type("b", Millis{100}).type("c", Millis{200}).build();
            const ErrorMap errors = errors_of(log, "ab");

            EXPECT_EQ(errors.insertions.at("c"), 1U);
            EXPECT_TRUE(errors.substitutions.empty());
            EXPECT_TRUE(errors.omissions.empty());
        }

        TEST(ErrorMapTest, MultiBytePairsKeyCorrectly) {
            const KeystrokeLog log = testing::LogBuilder{}.type("c", Millis{0}).type("😀", Millis{100}).build();
            const ErrorMap errors = errors_of(log, "čš");

            EXPECT_EQ(errors.substitutions.at(pair_of("č", "c")), 1U);
            EXPECT_EQ(errors.substitutions.at(pair_of("š", "😀")), 1U);
        }

        TEST(ErrorMapTest, APerfectRunHasNoErrorsOfAnyKind) {
            constexpr std::string_view text = "the quick brown fox";

            EXPECT_TRUE(errors_of(testing::perfect(text, Wpm{60.0}), text).empty());
        }

        TEST(ErrorMapTest, AnEmptyLogIsAnEmptyMap) { EXPECT_TRUE(errors_of(KeystrokeLog{}, "abc").empty()); }

        TEST(ErrorMapTest, AnUnfinishedRunIsNotAPageOfOmissions) {
            constexpr std::string_view text = "the quick brown fox jumps over the lazy dog";
            const KeystrokeLog log = testing::LogBuilder{}.type("t", Millis{0}).type("h", Millis{100}).build();

            EXPECT_TRUE(errors_of(log, text).empty()) << "stopping early is unfinished, not wrong";
        }

    }  // namespace
}  // namespace typeit::core
