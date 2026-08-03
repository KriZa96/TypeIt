#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/LogBuilder.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        constexpr std::string_view kText =
                "the quick brown fox jumps over the lazy dog while the band played on and on";

        /// GAMEPLAY section 4.1's gross WPM, computed here rather than imported:
        /// TI-036 does not exist yet, and when it does this is the number it has
        /// to reproduce.
        double gross_wpm(const KeystrokeLog& log, std::string_view text) {
            const TextBuffer target = testing::text_of(text);
            std::size_t correct = 0;
            for (const Keystroke& event: log.events()) {
                if (event.kind == KeystrokeKind::Character && event.target < target.size() &&
                    event.typed == target.at(GraphemeIndex{event.target})) {
                    ++correct;
                }
            }
            const double minutes = static_cast<double>(log.duration().value) / 60'000.0;
            return minutes > 0.0 ? (static_cast<double>(correct) / 5.0) / minutes : 0.0;
        }

        std::size_t first_attempt_errors(const KeystrokeLog& log, std::string_view text) {
            const TextBuffer target = testing::text_of(text);
            std::vector<bool> attempted(target.size(), false);
            std::size_t errors = 0;
            for (const Keystroke& event: log.events()) {
                if (event.kind != KeystrokeKind::Character || event.target >= target.size()) {
                    continue;
                }
                if (attempted[event.target]) {
                    continue;
                }
                attempted[event.target] = true;
                if (!(event.typed == target.at(GraphemeIndex{event.target}))) {
                    ++errors;
                }
            }
            return errors;
        }

        void expect_monotonic(const KeystrokeLog& log) {
            const std::span<const Keystroke> events = log.events();
            for (std::size_t i = 1; i < events.size(); ++i) {
                EXPECT_LE(events[i - 1].at, events[i].at) << "event " << i;
            }
        }

        TEST(LogBuilderTest, TheDslRecordsWhatItWasTold) {
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("h", Millis{100})
                                             .type("q", Millis{200})
                                             .backspace(Millis{300})
                                             .type("e", Millis{400})
                                             .build();

            const std::span<const Keystroke> events = log.events();
            ASSERT_EQ(events.size(), 4U);
            EXPECT_EQ(events[0].typed.view(), "h");
            EXPECT_EQ(events[1].typed.view(), "q");
            EXPECT_EQ(events[2].kind, KeystrokeKind::Backspace);
            EXPECT_EQ(events[3].typed.view(), "e");
            EXPECT_EQ(log.duration(), Millis{300});
        }

        TEST(LogBuilderTest, TheDslMovesTheCursorTheWayTheModelDoes) {
            const KeystrokeLog log = testing::LogBuilder{}
                                             .type("a", Millis{10})
                                             .type("b", Millis{20})
                                             .backspace(Millis{30})
                                             .type("b", Millis{40})
                                             .build();

            const std::span<const Keystroke> events = log.events();
            const std::vector<std::uint32_t> targets{events[0].target, events[1].target, events[2].target,
                                                     events[3].target};
            EXPECT_EQ(targets, (std::vector<std::uint32_t>{0, 1, 1, 1}));
        }

        TEST(LogBuilderTest, ABackspaceAtTheStartDoesNotUnderflow) {
            const KeystrokeLog log = testing::LogBuilder{}.backspace(Millis{10}).build();

            ASSERT_EQ(log.size(), 1U);
            EXPECT_EQ(log.events()[0].target, 0U);
        }

        TEST(LogBuilderTest, PerfectProducesTheSpeedItWasAskedFor) {
            for (const double speed: {20.0, 60.0, 120.0}) {
                const KeystrokeLog log = testing::perfect(kText, Wpm{speed});

                EXPECT_NEAR(gross_wpm(log, kText), speed, 0.5) << speed << " WPM";
                EXPECT_EQ(first_attempt_errors(log, kText), 0U);
                expect_monotonic(log);
            }
        }

        TEST(LogBuilderTest, PerfectTypesEveryGraphemeOnce) {
            const KeystrokeLog log = testing::perfect(kText, Wpm{60.0});

            EXPECT_EQ(log.size(), testing::text_of(kText).size());
        }

        TEST(LogBuilderTest, PerfectSurvivesTheDegenerateTexts) {
            EXPECT_EQ(testing::perfect("", Wpm{60.0}).size(), 0U);
            EXPECT_EQ(testing::perfect("", Wpm{60.0}).duration(), Millis{0});

            const KeystrokeLog one = testing::perfect("a", Wpm{60.0});
            EXPECT_EQ(one.size(), 1U);
            EXPECT_EQ(one.duration(), Millis{0}) << "one keystroke spans no time";
        }

        TEST(LogBuilderTest, WithErrorsProducesExactlyTheErrorsAskedFor) {
            for (const std::size_t errors: {0U, 1U, 3U, 10U}) {
                const KeystrokeLog log = testing::with_errors(kText, errors);

                EXPECT_EQ(first_attempt_errors(log, kText), errors) << errors << " errors";
                EXPECT_EQ(log.size(), testing::text_of(kText).size());
                expect_monotonic(log);
            }
        }

        TEST(LogBuilderTest, WithErrorsSpreadsThemThroughTheText) {
            // Bunched at the front they would be invisible to any metric that
            // looks at a window or a bucket.
            const KeystrokeLog log = testing::with_errors(kText, 4);
            const TextBuffer target = testing::text_of(kText);

            std::size_t last_error = 0;
            for (const Keystroke& event: log.events()) {
                if (!(event.typed == target.at(GraphemeIndex{event.target}))) {
                    last_error = event.target;
                }
            }
            EXPECT_GT(last_error, target.size() / 2) << "the last error is in the second half";
        }

        TEST(LogBuilderTest, BurstyHasIdenticalTotalsToPerfect) {
            // The premise of TI-038's even-versus-bursty comparison: if these
            // two differ in anything but their spacing, that test would be
            // measuring the wrong thing.
            const KeystrokeLog even = testing::perfect(kText, Wpm{60.0});
            const KeystrokeLog uneven = testing::bursty(kText, Wpm{60.0});

            EXPECT_EQ(uneven.size(), even.size());
            EXPECT_EQ(uneven.duration(), even.duration());
            EXPECT_NEAR(gross_wpm(uneven, kText), gross_wpm(even, kText), 0.001);
            expect_monotonic(uneven);
        }

        TEST(LogBuilderTest, BurstyActuallyBursts) {
            const KeystrokeLog uneven = testing::bursty(kText, Wpm{60.0});
            const std::span<const Keystroke> events = uneven.events();
            ASSERT_GT(events.size(), 10U);

            Millis shortest{events[1].at - events[0].at};
            Millis longest = shortest;
            for (std::size_t i = 1; i < events.size(); ++i) {
                const Millis gap = events[i].at - events[i - 1].at;
                shortest = std::min(shortest, gap);
                longest = std::max(longest, gap);
            }
            EXPECT_GT(longest.value, shortest.value * 3) << "an even run would have no such spread";
        }

        TEST(LogBuilderTest, WithPausesAddsExactlyTheIdleTime) {
            const std::vector<Millis> gaps{Millis{2'000}, Millis{5'000}};
            const KeystrokeLog even = testing::perfect(kText, Wpm{60.0});
            const KeystrokeLog paused = testing::with_pauses(kText, gaps);

            EXPECT_EQ(paused.size(), even.size());
            EXPECT_EQ(paused.duration(), even.duration() + Millis{7'000});
            expect_monotonic(paused);
        }

        TEST(LogBuilderTest, WithNoPausesIsJustPerfect) {
            const KeystrokeLog paused = testing::with_pauses(kText, {});

            EXPECT_EQ(paused.duration(), testing::perfect(kText, Wpm{60.0}).duration());
        }

    }  // namespace
}  // namespace typeit::core
