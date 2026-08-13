// Where the typist actually ran out of road (TI-128).
//
// "You were caught at 84 WPM" is a score. The useful question is why, and the
// answer is the band accuracy collapsed in and what was failing inside it —
// which is a *different* list from the whole run's worst pairs, and that
// difference is the entire point of the feature.
//
// The case that matters most is the one that must NOT report a wall: a
// transient dip that recovers. Live, "accuracy has dropped" and "accuracy has
// dropped and will recover in two seconds" look identical, which is why this is
// computed after the run and not during it.

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/race/Pacer.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/race/SpeedWall.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// A short accuracy window, so a scripted run does not need hundreds of
        /// keystrokes to move the gate.
        [[nodiscard]] RaceParams params() {
            RaceParams tuned;
            tuned.accuracy_window = 10;
            return tuned;
        }

        /// Builds a run: `target` is what the text says, `typed` is what the
        /// typist produced, one grapheme every 100 ms.
        class ScriptedRun {
        public:
            explicit ScriptedRun(std::string_view text) : target_{build(text)}, model_{target_} {}

            void type(std::string_view produced) {
                const TextBuffer keys = build(produced);
                for (const Grapheme& grapheme: keys.graphemes()) {
                    at_ = Millis{at_.value + 100};
                    model_.type(grapheme, at_);
                }
            }

            [[nodiscard]] const KeystrokeLog& log() const { return model_.log(); }
            [[nodiscard]] const TextBuffer& target() const { return target_; }

        private:
            [[nodiscard]] static TextBuffer build(std::string_view text) {
                Result<TextBuffer> made = TextBuffer::from_utf8(text);
                EXPECT_TRUE(made) << (made ? "" : made.error().context);
                return std::move(*made);
            }

            TextBuffer target_;
            TypingModel model_;
            Millis at_{0};
        };

        /// A ghost that climbs steadily, one reading a second.
        [[nodiscard]] std::vector<PacerSample> a_climbing_curve(std::int64_t seconds, double from, double per_second) {
            std::vector<PacerSample> curve;
            for (std::int64_t second = 0; second < seconds; ++second) {
                curve.push_back(PacerSample{.at = Millis{second * 1'000},
                                            .wpm = Wpm{from + (per_second * static_cast<double>(second))}});
            }
            return curve;
        }

        // ---- no collapse, no wall ------------------------------------------------

        TEST(SpeedWallTest, ARunWithNoCollapseReportsNoWallRatherThanInventingOne) {
            // Being caught is not the same as hitting a wall. Somebody can be
            // overtaken while typing perfectly, and a diagnosis for that would
            // be a made-up one.
            ScriptedRun run{"abcdefghijklmnopqrstuvwxyz"};
            run.type("abcdefghijklmnopqrstuvwxyz");

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(4, 40.0, 5.0), params());

            EXPECT_FALSE(wall.found());
            EXPECT_FALSE(wall.low.has_value());
            EXPECT_TRUE(wall.pairs.empty());
        }

        TEST(SpeedWallTest, AnEmptyRunHasNoWall) {
            ScriptedRun run{"abcdef"};

            const SpeedWall wall = speed_wall(run.log(), run.target(), {}, params());

            EXPECT_FALSE(wall.found());
        }

        // ---- the transient dip -------------------------------------------------------

        TEST(SpeedWallTest, ADipThatRecoversIsNotTheWall) {
            // The case this is really for. A handful of mistakes in the middle
            // of a run, then twenty clean keystrokes: accuracy came back, so
            // there was no wall.
            ScriptedRun run{"aaaaaaaaaabbbbbccccccccccccccccccccccccc"};
            run.type("aaaaaaaaaa");  // clean
            run.type("xxxxx");  // five wrong: the dip
            run.type("ccccccccccccccccccccccccc");  // and a long clean recovery

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(8, 40.0, 5.0), params());

            EXPECT_FALSE(wall.found()) << "accuracy recovered, so nothing was standing in the way";
        }

        TEST(SpeedWallTest, ACollapseAtTheEndIsTheWall) {
            ScriptedRun run{"aaaaaaaaaaaaaaaaaaaabbbbbbbbbb"};
            run.type("aaaaaaaaaaaaaaaaaaaa");  // twenty clean
            run.type("xxxxxxxxxx");  // then it falls apart and stays that way

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(6, 40.0, 5.0), params());

            ASSERT_TRUE(wall.found());
            ASSERT_TRUE(wall.began.has_value());
            EXPECT_GT(wall.began->value, 0) << "it did not begin at the first keystroke";
        }

        TEST(SpeedWallTest, ARunThatWasNeverAccurateWallsFromTheStart) {
            ScriptedRun run{"aaaaaaaaaaaaaaa"};
            run.type("xxxxxxxxxxxxxxx");

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(4, 40.0, 5.0), params());

            ASSERT_TRUE(wall.found());
            ASSERT_TRUE(wall.began.has_value());
            EXPECT_EQ(wall.began->value, 0) << "there was never a moment it was going well";
        }

        // ---- the band -------------------------------------------------------------------

        TEST(SpeedWallTest, TheBandIsTheSpeedsTheGhostHeldWhileAccuracyWasGone) {
            ScriptedRun run{"aaaaaaaaaaaaaaaaaaaabbbbbbbbbb"};
            run.type("aaaaaaaaaaaaaaaaaaaa");
            run.type("xxxxxxxxxxx");

            // Two seconds of clean typing, then the collapse: the ghost is at
            // 40, 45, 50 across the run.
            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(3, 40.0, 5.0), params());

            ASSERT_TRUE(wall.found());
            ASSERT_TRUE(wall.high.has_value());
            EXPECT_LE(wall.low->value, wall.high->value);
            EXPECT_GE(wall.low->value, 40.0) << "the band starts no earlier than the run does";
        }

        TEST(SpeedWallTest, ACollapseTheCurveSaysNothingAboutLeavesTheBandEmpty) {
            // A race that ended inside one second, so there is no reading
            // after the collapse began. The pairs still come back — those are
            // what the drill is built from — but a WPM band is not invented
            // from nothing.
            ScriptedRun run{"aaaaaaaaaaaaaaa"};
            run.type("xxxxxxxxxxxxxxx");

            const SpeedWall wall = speed_wall(run.log(), run.target(), {}, params());

            EXPECT_FALSE(wall.low.has_value());
            EXPECT_FALSE(wall.pairs.empty()) << "what failed is still known even when the speed is not";
        }

        // ---- the pairs, which are the point --------------------------------------------------

        TEST(SpeedWallTest, ThePairsAreTheOnesInsideTheBandAndNotTheWholeRuns) {
            // The fixture is built to make the two lists differ, because a
            // feature that returns the same answer as the cheap version is a
            // feature doing nothing.
            //
            // Twenty clean-ish keystrokes with one repeated early mistake, then
            // a collapse made entirely of a different mistake.
            ScriptedRun run{"qqqqqqqqqqqqqqqqqqqqzzzzzzzzzzzz"};
            run.type("wwwwwww");  // seven early `q`->`w`
            run.type("qqqqqqqqqqqqq");  // then clean, so accuracy recovers
            run.type("mmmmmmmmmmmm");  // then a lasting collapse of `z`->`m`

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(6, 40.0, 5.0), params());

            ASSERT_TRUE(wall.found());
            ASSERT_FALSE(wall.pairs.empty());
            EXPECT_EQ(wall.pairs.front().expected, "z");
            EXPECT_EQ(wall.pairs.front().typed, "m");
            for (const WallPair& pair: wall.pairs) {
                EXPECT_NE(pair.typed, "w") << "the early mistake is not what was limiting them";
            }
        }

        TEST(SpeedWallTest, AtMostFivePairsComeBackMostFrequentFirst) {
            // A list somebody reads before a drill, not a dataset.
            ScriptedRun run{"abcdefghijklmnopqrstuvwxyzabcdef"};
            run.type("111111112222222333333444455667788");

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(6, 40.0, 5.0), params());

            ASSERT_TRUE(wall.found());
            EXPECT_LE(wall.pairs.size(), kWallPairs);
            for (std::size_t at = 1; at < wall.pairs.size(); ++at) {
                EXPECT_GE(wall.pairs[at - 1].count, wall.pairs[at].count) << "at " << at;
            }
        }

        TEST(SpeedWallTest, ACorrectedMistakeStillCountsAsOne) {
            // First-attempt accuracy is what the gate reads during the race, so
            // the analysis afterwards has to agree with what the ramp was
            // reacting to at the time.
            ScriptedRun run{"aaaaaaaaaaaaaaa"};
            run.type("aaaaaaaaaa");
            run.type("xxxxx");

            const SpeedWall wall = speed_wall(run.log(), run.target(), a_climbing_curve(4, 40.0, 5.0), params());

            ASSERT_TRUE(wall.found());
            ASSERT_FALSE(wall.pairs.empty());
            EXPECT_EQ(wall.pairs.front().expected, "a");
            EXPECT_EQ(wall.pairs.front().typed, "x");
        }

    }  // namespace
}  // namespace typeit::core
