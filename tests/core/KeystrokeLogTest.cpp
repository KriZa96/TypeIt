#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Preconditions.h"

namespace typeit::core {
    namespace {

        /// The first grapheme of `text`, so a test can write "č" instead of five
        /// lines of array initialisation.
        Grapheme grapheme_of(std::string_view text) {
            const Result<TextBuffer> buffer = TextBuffer::from_utf8(text);
            EXPECT_TRUE(buffer) << text;
            EXPECT_EQ(buffer->size(), 1U) << text;
            return buffer->at(GraphemeIndex{0});
        }

        Keystroke typed(std::string_view text, std::int64_t at, std::uint32_t target = 0) {
            return Keystroke{
                    .at = Millis{at}, .target = target, .kind = KeystrokeKind::Character, .typed = grapheme_of(text)};
        }

        Keystroke backspace(std::int64_t at, std::uint32_t target = 0) {
            return Keystroke{.at = Millis{at}, .target = target, .kind = KeystrokeKind::Backspace, .typed = {}};
        }

        // ADR-002's guarantee, as a compile-time fact rather than a promise in a
        // comment: nothing but append can change the log, and nothing can take an
        // event back out of it. A metric that can be recomputed later depends on
        // this holding for every caller, including the ones not written yet.
        //
        // The concepts are templates because a requires-expression naming a
        // member that does not exist is a hard error where nothing is dependent.
        template<typename Log>
        concept Clearable = requires(Log log) { log.clear(); };
        template<typename Log>
        concept Poppable = requires(Log log) { log.pop_back(); };
        template<typename Log>
        concept Erasable = requires(Log log) { log.erase(0); };
        template<typename Log>
        concept Rewindable = requires(Log log) { log.rewind(); };
        template<typename Log>
        concept HandsOutAMutableEvent = requires(Log log, Keystroke event) { log.events()[0] = event; };

        static_assert(std::is_same_v<decltype(std::declval<KeystrokeLog&>().events()), std::span<const Keystroke>>,
                      "events() must not hand out a mutable view");
        static_assert(!Clearable<KeystrokeLog>);
        static_assert(!Poppable<KeystrokeLog>);
        static_assert(!Erasable<KeystrokeLog>);
        static_assert(!Rewindable<KeystrokeLog>);
        static_assert(!HandsOutAMutableEvent<KeystrokeLog>);

        TEST(KeystrokeLogTest, AnEmptyLogIsEmptyAndTookNoTime) {
            const KeystrokeLog log;

            EXPECT_EQ(log.size(), 0U);
            EXPECT_TRUE(log.empty());
            EXPECT_TRUE(log.events().empty());
            EXPECT_EQ(log.duration(), Millis{0});
        }

        TEST(KeystrokeLogTest, AppendsPreserveOrder) {
            KeystrokeLog log;
            log.append(typed("h", 100, 0));
            log.append(typed("q", 200, 1));
            log.append(backspace(300, 1));
            log.append(typed("e", 400, 1));

            ASSERT_EQ(log.size(), 4U);
            EXPECT_FALSE(log.empty());

            const std::span<const Keystroke> events = log.events();
            EXPECT_EQ(events[0].typed.view(), "h");
            EXPECT_EQ(events[1].typed.view(), "q");
            EXPECT_EQ(events[2].kind, KeystrokeKind::Backspace);
            EXPECT_EQ(events[3].typed.view(), "e");
            EXPECT_EQ(events[3].at, Millis{400});
            EXPECT_EQ(events[3].target, 1U);
        }

        TEST(KeystrokeLogTest, ABackspaceIsAnEventNotAnUndo) {
            // The distinction ADR-002 rests on: correcting a mistake adds history
            // rather than rewriting it, which is why a corrected error can still
            // be counted as an error (TI-037) and still appear in the error map.
            KeystrokeLog log;
            log.append(typed("x", 10, 0));
            log.append(backspace(20, 0));
            log.append(typed("a", 30, 0));

            EXPECT_EQ(log.size(), 3U);
            EXPECT_EQ(log.events()[0].typed.view(), "x");
        }

        TEST(KeystrokeLogTest, DurationIsLastMinusFirst) {
            KeystrokeLog log;
            log.append(typed("a", 1'000));
            log.append(typed("b", 1'500));
            log.append(typed("c", 61'000));

            EXPECT_EQ(log.duration(), Millis{60'000});
        }

        TEST(KeystrokeLogTest, ASingleEventTookNoTime) {
            // Not "the time until now": the log knows nothing about now. A caller
            // that wants elapsed-to-now has a clock and can subtract itself.
            KeystrokeLog log;
            log.append(typed("a", 5'000));

            EXPECT_EQ(log.duration(), Millis{0});
        }

        TEST(KeystrokeLogTest, RepeatedTimestampsAreAllowed) {
            // Two keys inside one millisecond is a fast typist, not an error, and
            // a paste arrives as several events at the same instant.
            KeystrokeLog log;
            log.append(typed("a", 100, 0));
            log.append(typed("b", 100, 1));

            EXPECT_EQ(log.size(), 2U);
            EXPECT_EQ(log.duration(), Millis{0});
        }

        TEST(KeystrokeLogTestDeath, TimestampsMustNotGoBackwards) {
            KeystrokeLog log;
            log.append(typed("a", 200));

            TYPEIT_EXPECT_PRECONDITION(log.append(typed("b", 199)), "backwards");
        }

        TEST(KeystrokeLogTest, GrowthDoesNotChangeAnyRecordedValue) {
            // The events are values, not references into a buffer, so a
            // reallocation cannot alter what was recorded. Asserted because the
            // whole design assumes a metric computed today and recomputed after a
            // thousand more keystrokes gets the same answer.
            KeystrokeLog log;
            std::vector<Keystroke> expected;
            for (std::int64_t i = 0; i < 1'000; ++i) {
                const Keystroke event = typed("a", i * 10, static_cast<std::uint32_t>(i));
                log.append(event);
                expected.push_back(event);
            }

            ASSERT_EQ(log.size(), expected.size());
            for (std::size_t i = 0; i < expected.size(); ++i) {
                EXPECT_EQ(log.events()[i], expected[i]) << "event " << i;
            }
            EXPECT_EQ(log.duration(), Millis{9'990});
        }

        TEST(KeystrokeLogTest, ReserveChangesNothingObservable) {
            KeystrokeLog reserved;
            reserved.reserve(1'000);
            KeystrokeLog plain;

            for (std::int64_t i = 0; i < 100; ++i) {
                reserved.append(typed("a", i, static_cast<std::uint32_t>(i)));
                plain.append(typed("a", i, static_cast<std::uint32_t>(i)));
            }

            EXPECT_GE(reserved.capacity(), 1'000U);
            EXPECT_EQ(reserved.size(), plain.size());
            EXPECT_EQ(reserved.duration(), plain.duration());
            EXPECT_TRUE(std::ranges::equal(reserved.events(), plain.events()));
        }

        TEST(KeystrokeLogTest, AHundredThousandEventsStayWithinTheMemoryBound) {
            // ADR-002's cost line, checked. 32 bytes an event with a vector's
            // doubling gives 64 bytes an event worst case; the log of an
            // implausibly long run is a few megabytes, and this fails loudly if
            // an event ever grows an out-of-line member.
            constexpr std::size_t kEvents = 100'000;
            constexpr std::size_t kBytesPerEventBound = 2 * sizeof(Keystroke);

            KeystrokeLog log;
            for (std::size_t i = 0; i < kEvents; ++i) {
                log.append(typed("a", static_cast<std::int64_t>(i), static_cast<std::uint32_t>(i)));
            }

            ASSERT_EQ(log.size(), kEvents);
            EXPECT_LE(log.capacity() * sizeof(Keystroke), kEvents * kBytesPerEventBound);
            EXPECT_LE(sizeof(Keystroke), 32U);
        }

    }  // namespace
}  // namespace typeit::core
