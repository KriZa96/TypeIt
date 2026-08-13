// What a race leaves behind (TI-127).
//
// A race records three things no other mode has: the highest speed it held, the
// speed accuracy collapsed at, and the ghost's curve. It also has to record the
// numbers it was *run* by — a preset edited next week must not rewrite what last
// week's race was, and a config file is not a record of anything.

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/modes/RaceMode.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::app {
    namespace {

        constexpr core::Millis kStart{1'767'225'600'000};

        /// Long enough that a race does not run out of text before the ghost
        /// catches up.
        [[nodiscard]] std::string a_long_text() {
            std::string text;
            while (text.size() < 3'000) {
                text += "the quick brown fox jumps over the lazy dog. ";
            }
            return text;
        }

        class RacePersistenceTest : public ::testing::Test {
        protected:
            RacePersistenceTest() {
                modes_.register_mode(
                        "race", [] { return std::make_unique<core::RaceMode>(core::RaceParams{}, core::Wpm{40.0}); });
            }

            [[nodiscard]] SessionRequest a_race() {
                SessionRequest request;
                request.mode = "race";
                request.text = text_;
                request.seed = 4'242;
                return request;
            }

            /// Types `count` graphemes correctly, one every `gap`, ticking as
            /// it goes so the ghost moves and the ramp runs.
            void race_for(const ActiveRun& run, std::size_t count, core::Millis gap) {
                const core::TextBuffer typed = testing::text_of(text_);
                for (std::size_t done = 0; done < count && done < typed.size(); ++done) {
                    at_ = core::Millis{at_.value + gap.value};
                    run.session->on_key(typed.at(core::GraphemeIndex{done}), at_);
                    run.session->on_tick(at_);
                }
            }

            std::string text_ = a_long_text();
            testing::FakeClock clock_{kStart};
            testing::FakeHistoryRepository history_;
            core::ModeRegistry modes_;
            SessionService service_{history_, modes_, clock_, clock_};
            core::Millis at_{kStart};
        };

        // ---- the two race-only numbers ---------------------------------------------

        TEST_F(RacePersistenceTest, ARaceRecordsTheSpeedItHeld) {
            core::Result<ActiveRun> run = service_.start(a_race());
            ASSERT_TRUE(run) << (run ? "" : run.error().context);
            // Steady and accurate for long enough that the ramp climbs and a
            // speed is held past the sustain window.
            race_for(*run, 400, core::Millis{150});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved) << (saved ? "" : saved.error().context);
            ASSERT_TRUE(saved->record.peak_wpm.has_value()) << "a race without a peak is a race nobody can beat";
            EXPECT_GT(saved->record.peak_wpm->value, 0.0);
        }

        TEST_F(RacePersistenceTest, ARaceWithNoCollapseHasNoWallRatherThanAWallOfZero) {
            // A wall at 0 WPM is a mark on a chart saying nothing happened.
            core::Result<ActiveRun> run = service_.start(a_race());
            ASSERT_TRUE(run);
            race_for(*run, 200, core::Millis{150});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved) << (saved ? "" : saved.error().context);
            EXPECT_FALSE(saved->record.wall_wpm.has_value());
        }

        TEST_F(RacePersistenceTest, AModeThatIsNotARaceRecordsNeitherOfThem) {
            // Otherwise every timed run carries two empty race columns and a
            // chart with an empty second series on it.
            modes_.register_mode("quote", [] { return std::make_unique<core::QuoteMode>(); });
            SessionRequest request;
            request.mode = "quote";
            request.text = "hello world";
            core::Result<ActiveRun> run = service_.start(request);
            ASSERT_TRUE(run) << (run ? "" : run.error().context);
            race_for(*run, 11, core::Millis{200});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved);
            EXPECT_FALSE(saved->record.peak_wpm.has_value());
            EXPECT_FALSE(saved->record.wall_wpm.has_value());
        }

        // ---- the ghost's curve --------------------------------------------------------

        TEST_F(RacePersistenceTest, TheTimelineCarriesThePacersSpeedAlongsideTheTypists) {
            core::Result<ActiveRun> run = service_.start(a_race());
            ASSERT_TRUE(run);
            race_for(*run, 400, core::Millis{150});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved) << (saved ? "" : saved.error().context);
            ASSERT_FALSE(saved->record.timeline.empty());
            const bool any_pacer = std::ranges::any_of(saved->record.timeline, [](const core::TimelineSample& sample) {
                return sample.pacer_wpm.has_value();
            });
            EXPECT_TRUE(any_pacer) << "the ghost was in the race and should be on the chart";
        }

        TEST_F(RacePersistenceTest, ATimedRunsTimelineCarriesNoPacerAtAll) {
            modes_.register_mode("quote", [] { return std::make_unique<core::QuoteMode>(); });
            SessionRequest request;
            request.mode = "quote";
            request.text = "hello world";
            core::Result<ActiveRun> run = service_.start(request);
            ASSERT_TRUE(run);
            race_for(*run, 11, core::Millis{200});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved);
            for (const core::TimelineSample& sample: saved->record.timeline) {
                EXPECT_FALSE(sample.pacer_wpm.has_value());
            }
        }

        // ---- the parameters it was run by ------------------------------------------------

        TEST_F(RacePersistenceTest, TheRunRecordsEveryParameterItWasPlayedWith) {
            // So a race stays reconstructible after somebody edits a preset.
            // Reading the config back at display time would mean last week's
            // race quietly re-describing itself as this week's difficulty.
            core::Result<ActiveRun> run = service_.start(a_race());
            ASSERT_TRUE(run);
            race_for(*run, 60, core::Millis{150});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved) << (saved ? "" : saved.error().context);
            const std::string& param = saved->record.mode_param;
            for (const std::string_view field:
                 {"ramp_up", "ramp_down", "min_accuracy", "lead_comfort", "lead_danger", "lead_scale", "grace_ms",
                  "lives", "catch_penalty", "start_factor", "sustain_window_ms", "min_speed", "max_speed"}) {
                EXPECT_NE(param.find(field), std::string::npos) << field << " missing from " << param;
            }
            EXPECT_NE(param.find("\"kind\":\"race\""), std::string::npos) << param;
        }

        TEST_F(RacePersistenceTest, AModeParamTheCallerSuppliedIsNotOverwritten) {
            // The caller knows things this does not — which preset was chosen
            // by name, for one — so it says so and this does not argue.
            SessionRequest request = a_race();
            request.mode_param = R"({"preset":"brutal"})";
            core::Result<ActiveRun> run = service_.start(request);
            ASSERT_TRUE(run);
            race_for(*run, 40, core::Millis{150});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(saved);
            EXPECT_EQ(saved->record.mode_param, R"({"preset":"brutal"})");
        }

        // ---- personal bests -----------------------------------------------------------------

        TEST_F(RacePersistenceTest, AnAbandonedRaceSetsNoBest) {
            core::Result<ActiveRun> run = service_.start(a_race());
            ASSERT_TRUE(run);
            race_for(*run, 400, core::Millis{150});

            const core::Result<SessionResult> saved = service_.finish(*run, Outcome::Abandoned);

            ASSERT_TRUE(saved) << (saved ? "" : saved.error().context);
            EXPECT_FALSE(saved->record.completed);
            EXPECT_TRUE(history_.personal_bests().value_or(std::vector<PersonalBest>{}).empty())
                    << "a race somebody walked out of is not a record";
        }

    }  // namespace
}  // namespace typeit::app
