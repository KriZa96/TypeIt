// The orchestration, over fakes and a clock that does not move by itself.
//
// What is only testable here: that a finished run is written exactly once and
// whole, that an abandoned one is written and earns nothing, and that the
// service keeps no state between two runs. The metrics themselves are core's
// and are tested there; this asserts that the right ones reach the record.
//
// The intra-transaction half of TI-068's acceptance — a failure *after* the
// session row leaves no partial write — is in HistoryRepositoryContract, where
// both the real adapter and the fake can be made to fail mid-write. At this
// level there is one persistence call by construction, which is the property
// under test.

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/core/Version.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/modes/TimedMode.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::app {
    namespace {

        /// Noon on 1 January 2026, the same fixed instant the repository tests
        /// use. Both clocks read it: a fake clock has one hand.
        constexpr core::Millis kStart{1'767'225'600'000};

        constexpr std::string_view kText = "hello world";

        class SessionServiceTest : public ::testing::Test {
        protected:
            SessionServiceTest() {
                modes_.register_mode("quote", [] { return std::make_unique<core::QuoteMode>(); });
                modes_.register_mode("timed", [] { return std::make_unique<core::TimedMode>(core::Millis{30'000}); });
            }

            [[nodiscard]] static SessionRequest a_request(std::string_view text = kText) {
                SessionRequest request;
                request.mode = "quote";
                request.mode_param = R"({"text":"fixture"})";
                request.text = text;
                request.seed = 4'242;
                return request;
            }

            /// Types `text` into the run, one grapheme every `gap`, starting
            /// from where the last keystroke left off.
            void type(const ActiveRun& run, std::string_view text, core::Millis gap = core::Millis{300}) {
                const core::TextBuffer typed = testing::text_of(text);
                for (const core::Grapheme& grapheme: typed.graphemes()) {
                    at_ += gap;
                    run.session->on_key(grapheme, at_);
                }
            }

            /// A whole run, typed perfectly, in one line — most tests here care
            /// about what was written rather than about how it was played.
            [[nodiscard]] core::Result<SessionResult> a_perfect_run(Outcome outcome = Outcome::Completed,
                                                                    core::Millis gap = core::Millis{300}) {
                core::Result<ActiveRun> run = service_.start(a_request());
                EXPECT_TRUE(run) << (run ? "" : run.error().message);
                if (!run) {
                    return std::unexpected{run.error()};
                }
                type(*run, kText, gap);
                return service_.finish(*run, outcome);
            }

            testing::FakeClock clock_{kStart};
            testing::FakeHistoryRepository history_;
            core::ModeRegistry modes_;
            SessionService service_{history_, modes_, clock_, clock_};
            /// Where the next keystroke lands, on the monotonic clock.
            core::Millis at_{kStart};
        };

        TEST_F(SessionServiceTest, ACompletedRunIsPersistedOnceWithItsSamplesAndItsKeyStats) {
            const core::Result<SessionResult> result = a_perfect_run();

            ASSERT_TRUE(result) << (result ? "" : result.error().message);
            ASSERT_EQ(history_.records.size(), 1U);
            const SessionRecord& saved = history_.records.front();

            EXPECT_TRUE(saved.completed);
            EXPECT_EQ(saved.mode, "quote");
            EXPECT_EQ(saved.mode_param, R"({"text":"fixture"})");
            EXPECT_EQ(saved.graphemes_typed, 11U);
            EXPECT_EQ(saved.graphemes_correct, 11U);
            EXPECT_EQ(saved.errors_total, 0U);
            EXPECT_EQ(saved.backspaces, 0U);
            // Eleven keystrokes 300 ms apart span 3 s, and the last one lands
            // exactly on a bucket boundary and is counted in the bucket it
            // closes — three samples, not four.
            EXPECT_EQ(saved.timeline.size(), 3U);
            EXPECT_EQ(history_.keys.per_grapheme.at("h").attempts, 1U);
            EXPECT_EQ(history_.keys.per_bigram.at("he").attempts, 1U);
        }

        TEST_F(SessionServiceTest, EverythingIsWrittenThroughOneCall) {
            // The transaction guarantee, as seen from above: the service never
            // reaches for `save` and the two merges separately, because three
            // calls cannot be made atomic from here.
            ASSERT_TRUE(a_perfect_run());

            EXPECT_EQ(history_.saves, 1U);
            EXPECT_EQ(history_.merges, 0U) << "the stats went in with the run, not after it";
        }

        TEST_F(SessionServiceTest, TheHeadlineMetricsComeFromTheLogRatherThanTheConfiguredDuration) {
            const core::Result<SessionResult> result = a_perfect_run();

            ASSERT_TRUE(result);
            const SessionRecord& saved = history_.records.front();
            EXPECT_EQ(saved.duration, core::Millis{3'000});
            EXPECT_DOUBLE_EQ(saved.accuracy.value, 1.0);
            EXPECT_DOUBLE_EQ(saved.final_correctness.value, 1.0);
            // 11 graphemes, 5 to a word, over 3 s.
            EXPECT_DOUBLE_EQ(saved.gross_wpm.value, 44.0);
            EXPECT_DOUBLE_EQ(saved.net_wpm.value, 44.0);
        }

        TEST_F(SessionServiceTest, AnUncorrectedMistakeIsCountedTwice) {
            // Once as a first-attempt error, which accuracy is about, and once
            // as something still wrong at the end, which the net WPM penalty is
            // about. Keeping the two apart is defect C4's whole fix.
            core::Result<ActiveRun> run = service_.start(a_request());
            ASSERT_TRUE(run);
            type(*run, "hxllo world");

            const core::Result<SessionResult> result = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(result);
            EXPECT_EQ(result->record.errors_total, 1U);
            EXPECT_EQ(result->record.errors_uncorrected, 1U);
            EXPECT_EQ(result->record.graphemes_correct, 10U);
        }

        TEST_F(SessionServiceTest, ACorrectedMistakeIsAnErrorThatNoLongerStands) {
            core::Result<ActiveRun> run = service_.start(a_request());
            ASSERT_TRUE(run);
            type(*run, "hx");
            at_ += core::Millis{300};
            run->session->on_backspace(at_);
            type(*run, "ello world");

            const core::Result<SessionResult> result = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(result);
            EXPECT_EQ(result->record.errors_total, 1U);
            EXPECT_EQ(result->record.errors_uncorrected, 0U);
            EXPECT_EQ(result->record.backspaces, 1U);
            EXPECT_DOUBLE_EQ(result->record.final_correctness.value, 1.0);
        }

        TEST_F(SessionServiceTest, AnAbandonedRunIsSavedAndEarnsNothing) {
            const core::Result<SessionResult> result = a_perfect_run(Outcome::Abandoned);

            ASSERT_TRUE(result);
            ASSERT_EQ(history_.records.size(), 1U) << "it happened, so it is history";
            EXPECT_FALSE(history_.records.front().completed);
            EXPECT_TRUE(history_.personal_bests().value_or(std::vector<PersonalBest>{}).empty());
        }

        TEST_F(SessionServiceTest, AFasterRunTakesTheRecordAndASlowerOneDoesNot) {
            ASSERT_TRUE(a_perfect_run(Outcome::Completed, core::Millis{300}));
            const core::SessionId fast = history_.rows.back().id;
            ASSERT_TRUE(a_perfect_run(Outcome::Completed, core::Millis{900}));

            const core::Result<std::vector<PersonalBest>> bests = history_.personal_bests();

            ASSERT_TRUE(bests);
            const auto net =
                    std::ranges::find_if(*bests, [](const PersonalBest& row) { return row.metric == "net_wpm"; });
            ASSERT_NE(net, bests->end());
            EXPECT_EQ(net->session_id, fast);
        }

        TEST_F(SessionServiceTest, TheProviderSeedIsRecordedAndReproducesTheText) {
            const core::Result<ActiveRun> first = service_.start(a_request());
            const core::Result<ActiveRun> second = service_.start(a_request());

            ASSERT_TRUE(first);
            ASSERT_TRUE(second);
            EXPECT_EQ(first->session->provider().seed(), 4'242U);
            EXPECT_EQ(first->session->text().to_string(), second->session->text().to_string());

            ASSERT_TRUE(a_perfect_run());
            EXPECT_EQ(history_.records.front().provider_seed, 4'242U);
            EXPECT_EQ(history_.records.front().provider, "whole");
        }

        TEST_F(SessionServiceTest, TheAppVersionIsReadRatherThanSpelled) {
            const core::Result<SessionResult> result = a_perfect_run();

            ASSERT_TRUE(result);
            EXPECT_EQ(result->record.app_version, kVersionString);
            EXPECT_FALSE(result->record.app_version.empty());
        }

        TEST_F(SessionServiceTest, AnUnknownModeIsRefusedAndNothingIsWritten) {
            SessionRequest request = a_request();
            request.mode = "telepathy";

            const core::Result<ActiveRun> run = service_.start(request);

            ASSERT_FALSE(run);
            EXPECT_EQ(run.error().code, core::ErrorCode::UnknownMode);
            EXPECT_TRUE(history_.records.empty());
            EXPECT_EQ(history_.saves, 0U);
        }

        TEST_F(SessionServiceTest, ARunWithNothingToTypeIsRefused) {
            const core::Result<ActiveRun> run = service_.start(a_request(""));

            ASSERT_FALSE(run);
            EXPECT_EQ(run.error().code, core::ErrorCode::EmptyText);
            EXPECT_TRUE(history_.records.empty());
        }

        TEST_F(SessionServiceTest, ABookmarkParkedAtTheEndIsRefusedRatherThanPlayedEmpty) {
            // The chunk is empty although the text is not, which is why the
            // guard reads the built session rather than the request.
            SessionRequest request = a_request();
            request.resume_at = core::GraphemeIndex{99};

            const core::Result<ActiveRun> run = service_.start(request);

            ASSERT_FALSE(run);
            EXPECT_EQ(run.error().code, core::ErrorCode::EmptyText);
        }

        TEST_F(SessionServiceTest, TextThatIsNotTextIsRefusedByTheByte) {
            const core::Result<ActiveRun> run = service_.start(a_request("ok\xffno"));

            ASSERT_FALSE(run);
            EXPECT_EQ(run.error().code, core::ErrorCode::InvalidUtf8);
        }

        TEST_F(SessionServiceTest, AChunkedRunTakesItsChunkAndSaysSo) {
            SessionRequest request = a_request("one two three four");
            request.resume_at = core::GraphemeIndex{0};
            request.chunk_graphemes = 7;

            core::Result<ActiveRun> run = service_.start(request);
            ASSERT_TRUE(run) << (run ? "" : run.error().message);
            EXPECT_EQ(run->session->text().to_string(), "one ");

            type(*run, "one ");
            const core::Result<SessionResult> result = service_.finish(*run, Outcome::Completed);

            ASSERT_TRUE(result);
            EXPECT_EQ(result->record.provider, "chunked");
        }

        TEST_F(SessionServiceTest, AFailedWriteLeavesTheHistoryExactlyAsItWas) {
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the disk is on fire"));

            const core::Result<SessionResult> result = a_perfect_run();

            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().code, core::ErrorCode::DbQuery);
            EXPECT_TRUE(history_.records.empty());
            EXPECT_TRUE(history_.keys.per_grapheme.empty()) << "no stats from a run that was not saved";
            EXPECT_TRUE(history_.errors.substitutions.empty());
            EXPECT_TRUE(history_.personal_bests().value_or(std::vector<PersonalBest>{}).empty());
        }

        TEST_F(SessionServiceTest, TwoSequentialSessionsShareNoState) {
            // The regression guard for the five global booleans 1.0 kept its
            // session state in: a second run starts empty however the first one
            // went, and the service itself remembers nothing between them.
            ASSERT_TRUE(a_perfect_run());

            core::Result<ActiveRun> second = service_.start(a_request());
            ASSERT_TRUE(second);
            type(*second, "h");
            const core::Result<SessionResult> result = service_.finish(*second, Outcome::Completed);

            ASSERT_TRUE(result);
            ASSERT_EQ(history_.records.size(), 2U);
            EXPECT_EQ(history_.records.back().graphemes_typed, 1U);
            EXPECT_EQ(history_.records.back().graphemes_correct, 1U);
            EXPECT_EQ(history_.keys.per_grapheme.at("h").attempts, 2U) << "the totals accumulate, the runs do not";
        }

        TEST_F(SessionServiceTest, ATimedRunEndsWhenItsModeSaysSo) {
            // The tick path, which nothing else here exercises: time passes
            // with nobody typing and the run is over.
            SessionRequest request = a_request();
            request.mode = "timed";

            core::Result<ActiveRun> run = service_.start(request);
            ASSERT_TRUE(run);
            type(*run, "h");
            EXPECT_FALSE(run->session->is_finished());

            at_ += core::Millis{30'000};
            run->session->on_tick(at_);

            EXPECT_TRUE(run->session->is_finished());
            EXPECT_TRUE(service_.finish(*run, Outcome::Completed));
            EXPECT_EQ(history_.records.front().mode, "timed");
        }

    }  // namespace
}  // namespace typeit::app
