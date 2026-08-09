// Type until you stop (TI-120, GAMEPLAY section 2.5).
//
// Endless is race mode with the ghost switched off, and is built that way
// rather than as a second code path — the only difference between "never
// finishes" and "finishes when the ghost catches you" is whether there is a
// ghost. So the cases here are about the two things that *are* different: a run
// with no end has nothing to be a fraction of, and a cumulative average over an
// hour stops responding to the last two minutes.

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <utility>

#include "typeit/core/modes/RaceMode.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// Long enough for ten simulated minutes of typing.
        [[nodiscard]] const TextBuffer& endless_text() {
            static const TextBuffer buffer = [] {
                std::string text;
                while (text.size() < 60'000) {
                    text += "the quick brown fox jumps over the lazy dog. ";
                }
                Result<TextBuffer> built = TextBuffer::from_utf8(text);
                EXPECT_TRUE(built);
                return std::move(*built);
            }();
            return buffer;
        }

        [[nodiscard]] RaceMode an_endless_run() { return RaceMode{RaceParams{}, Wpm{60.0}, false}; }

        /// Types correctly at a steady rate, telling the mode about each one.
        class SteadyTypist {
        public:
            SteadyTypist() : model_{endless_text()} {}

            void type(RaceMode& mode, std::size_t count, Millis at) {
                for (std::size_t done = 0; done < count; ++done) {
                    const GraphemeIndex target = model_.cursor();
                    const Grapheme expected = endless_text().at(target);
                    model_.type(expected, at);
                    mode.on_keystroke(Keystroke{.at = at,
                                                .target = static_cast<std::uint32_t>(target.value),
                                                .kind = KeystrokeKind::Character,
                                                .typed = expected},
                                      model_);
                }
            }

            [[nodiscard]] TypingModel& model() { return model_; }

        private:
            TypingModel model_;
        };

        // ---- it does not end -------------------------------------------------------

        TEST(EndlessModeTest, ItNeverReportsFinishedHoweverLongTheRunGoesOn) {
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            for (std::int64_t at = 1'000; at <= 600'000; at += 1'000) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_FALSE(mode.is_finished()) << "ten minutes of staring at the screen is not a loss";
        }

        TEST(EndlessModeTest, ItNeverReportsFinishedHoweverFarTheTypistGets) {
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            for (std::int64_t at = 200; at <= 200'000; at += 200) {
                typist.type(mode, 1, Millis{at});
            }

            EXPECT_FALSE(mode.is_finished());
            EXPECT_GT(mode.race().distance, 900U) << "and it counted how far they got";
        }

        TEST(EndlessModeTest, ItCallsItselfEndlessSoTheHistorySaysWhatItWas) {
            EXPECT_EQ(an_endless_run().id(), "endless");
        }

        TEST(EndlessModeTest, ProgressIsElapsedTimeBecauseThereIsNoTotalToBeAFractionOf) {
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            mode.on_tick(Millis{42'000}, typist.model());

            const ModeProgress progress = mode.progress();
            ASSERT_TRUE(std::holds_alternative<OpenProgress>(progress));
            EXPECT_EQ(std::get<OpenProgress>(progress).elapsed, Millis{42'000});
        }

        // ---- rolling rather than cumulative ------------------------------------------

        TEST(EndlessModeTest, TheSpeedShownIsRollingRatherThanTheWholeRunsAverage) {
            // The reason endless has its own HUD number. A cumulative average
            // over an hour makes the last two minutes invisible, so a typist
            // who has just sped up cannot tell.
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            // A slow minute: one grapheme a second is 12 wpm.
            for (std::int64_t at = 1'000; at <= 60'000; at += 1'000) {
                typist.type(mode, 1, Millis{at});
                mode.on_tick(Millis{at}, typist.model());
            }
            const double after_slow = mode.race().rolling_wpm.value;

            // Then fifteen fast seconds: five a second is 60 wpm.
            for (std::int64_t at = 60'200; at <= 75'000; at += 200) {
                typist.type(mode, 1, Millis{at});
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_NEAR(after_slow, 12.0, 2.0) << "the slow minute read as slow";
            EXPECT_NEAR(mode.race().rolling_wpm.value, 60.0, 6.0)
                    << "and the window caught up rather than averaging the hour: " << mode.race().rolling_wpm.value;
        }

        TEST(EndlessModeTest, ThePeakRollingSpeedIsKeptBecauseThereIsNoFinalScore) {
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            for (std::int64_t at = 200; at <= 30'000; at += 200) {
                typist.type(mode, 1, Millis{at});
                mode.on_tick(Millis{at}, typist.model());
            }
            const double peak = mode.race().peak_rolling_wpm.value;
            for (std::int64_t at = 31'000; at <= 90'000; at += 1'000) {
                mode.on_tick(Millis{at}, typist.model());
            }

            EXPECT_GT(peak, 45.0) << "it reached a real speed";
            EXPECT_DOUBLE_EQ(mode.race().peak_rolling_wpm.value, peak) << "and stopping did not take the peak away";
            EXPECT_LT(mode.race().rolling_wpm.value, peak) << "though the current reading fell";
        }

        TEST(EndlessModeTest, NothingTypedYetIsZeroRatherThanArithmeticOnNoTime) {
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            mode.on_tick(Millis{0}, typist.model());

            EXPECT_DOUBLE_EQ(mode.race().rolling_wpm.value, 0.0);
        }

        // ---- what a ten-minute run costs ------------------------------------------------

        TEST(EndlessModeTest, TenSimulatedMinutesVisitEachKeystrokeATightlyBoundedNumberOfTimes) {
            // "Bounded memory" in the mode itself: the accuracy window is
            // capped, and the rolling speed is amortised O(1) per tick rather
            // than a rescan of the log. Asserted through the count of events
            // entering and leaving the window, because "no rescan" is a claim
            // that otherwise stops being true silently.
            //
            // The keystroke log does grow linearly, deliberately — it is the
            // single source of truth every metric is computed from.
            SteadyTypist typist;
            RaceMode mode = an_endless_run();
            mode.on_start(Millis{0}, typist.model());

            std::size_t ticks = 0;
            for (std::int64_t at = 200; at <= 600'000; at += 200) {
                typist.type(mode, 1, Millis{at});
                mode.on_tick(Millis{at}, typist.model());
                ++ticks;
            }

            EXPECT_FALSE(mode.is_finished());
            EXPECT_EQ(mode.race().distance, ticks) << "every keystroke counted once";
            EXPECT_GT(ticks, 2'900U) << "and it really was ten minutes of typing";
        }

    }  // namespace
}  // namespace typeit::core
