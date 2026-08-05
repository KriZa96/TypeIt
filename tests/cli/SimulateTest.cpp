// A whole run, end to end, with no terminal in it.
//
// This is Phase 3's reason for existing: the same SessionService and the same
// core::Session a real run goes through, driven from a file. Nothing here is a
// stand-in for the game — only the repository and the clock are fakes, and
// both are ports the real application injects too.

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>

#include "typeit/app/services/SessionService.h"
#include "typeit/cli/Script.h"
#include "typeit/cli/Simulate.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/modes/TimedMode.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::cli {
    namespace {

        constexpr core::Millis kStart{1'767'225'600'000};
        constexpr std::string_view kAlphabet = "abcdefghijklmnopqrstuvwxyz";

        /// One `type` line per grapheme of `text`, a second apart, starting at
        /// one second. Written as a helper so a test says how the typing went
        /// rather than spelling out thirty lines of fixture.
        std::string script_of(std::string_view text) {
            std::string out = "# typeit-script 1\n";
            std::int64_t at = 1'000;
            for (const char letter: text) {
                out += std::to_string(at);
                out += letter == ' ' ? "  type space\n" : std::string{"  type "} + letter + "\n";
                at += 1'000;
            }
            return out;
        }

        class SimulateTest : public ::testing::Test {
        protected:
            SimulateTest() {
                modes_.register_mode("timed", [] { return std::make_unique<core::TimedMode>(core::Millis{30'000}); });
                modes_.register_mode("quote", [] { return std::make_unique<core::QuoteMode>(); });
            }

            [[nodiscard]] static app::SessionRequest a_request(std::string_view mode = "timed",
                                                               std::string_view text = kAlphabet) {
                app::SessionRequest request;
                request.mode = mode;
                request.mode_param = R"({"seconds":30})";
                request.text = text;
                request.seed = 4'242;
                return request;
            }

            [[nodiscard]] std::string run(const Script& script, const app::SessionRequest& request = a_request()) {
                const core::Result<std::string> output = simulate(service_, request, script);
                EXPECT_TRUE(output) << (output ? "" : output.error().context);
                return output.value_or(std::string{});
            }

            [[nodiscard]] static Script script_from(std::string_view text) {
                const core::Result<Script> script = parse_script(text);
                EXPECT_TRUE(script) << (script ? "" : script.error().context);
                return script.value_or(Script{});
            }

            testing::FakeClock clock_{kStart};
            testing::FakeHistoryRepository history_;
            core::ModeRegistry modes_;
            app::SessionService service_{history_, modes_, clock_, clock_};
        };

        TEST_F(SimulateTest, AThirtySecondTimedRunProducesTheHandComputedMetrics) {
            // Twenty-five letters, one a second, from 1 s to 25 s. The log spans
            // 24 s — first keystroke to last, never the configured duration,
            // which is defect C5 — so:
            //   0.4 minutes, 25 graphemes, 5 to a word  ->  12.5 WPM
            // and with nothing typed wrongly, raw, gross and net all agree.
            const std::string output = run(script_from(script_of(kAlphabet.substr(0, 25))));

            EXPECT_NE(output.find(R"("duration_ms":24000)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("graphemes_typed":25)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("graphemes_correct":25)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("errors_total":0)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("raw_wpm":12.5)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("gross_wpm":12.5)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("net_wpm":12.5)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("accuracy":1)"), std::string::npos) << output;
        }

        TEST_F(SimulateTest, ATimedRunEndsOnTheClockEvenIfTheTypistStopped) {
            // The script stops at 25 s of a 30-second run. The deadline still
            // arrives: the run is completed, not abandoned.
            const std::string output = run(script_from(script_of(kAlphabet.substr(0, 25))));

            EXPECT_NE(output.find(R"("completed":true)"), std::string::npos) << output;
            ASSERT_EQ(history_.records.size(), 1U);
            EXPECT_TRUE(history_.records.front().completed);
        }

        TEST_F(SimulateTest, AScriptCannotKeepTypingPastTheEndOfTheRun) {
            // Sixty letters at a second each would run to 60 s. The mode ends at
            // 30, and a file does not get to type past that any more than a
            // typist would.
            const std::string text(60, 'a');
            app::SessionRequest request = a_request();
            request.text = text;

            const std::string output = run(script_from(script_of(text)), request);

            EXPECT_NE(output.find(R"("completed":true)"), std::string::npos) << output;
            ASSERT_EQ(history_.records.size(), 1U);
            EXPECT_EQ(history_.records.front().graphemes_typed, 30U) << "one a second for thirty seconds";
        }

        TEST_F(SimulateTest, AQuoteRunThatStopsShortIsAbandonedAndSavedAnyway) {
            const std::string output = run(script_from(script_of("abc")), a_request("quote", kAlphabet));

            EXPECT_NE(output.find(R"("completed":false)"), std::string::npos) << output;
            ASSERT_EQ(history_.records.size(), 1U) << "it happened, so it is history";
            EXPECT_FALSE(history_.records.front().completed);
        }

        TEST_F(SimulateTest, AQuoteRunTypedThroughIsCompleted) {
            const std::string output = run(script_from(script_of(kAlphabet)), a_request("quote", kAlphabet));

            EXPECT_NE(output.find(R"("completed":true)"), std::string::npos) << output;
        }

        TEST_F(SimulateTest, TheSameScriptProducesByteIdenticalOutput) {
            // The determinism property, and the reason the output carries no
            // wall-clock timestamp: the clock is moved a day between the two
            // runs and the answer does not budge.
            const Script script = script_from(script_of(kAlphabet.substr(0, 25)));

            const std::string first = run(script);
            clock_.advance(core::Millis{86'400'000});
            const std::string second = run(script);

            EXPECT_EQ(first, second);
            EXPECT_FALSE(first.empty());
        }

        TEST_F(SimulateTest, NoWallClockTimestampReachesTheOutput) {
            const std::string output = run(script_from(script_of("abc")));

            EXPECT_EQ(output.find("started_at"), std::string::npos) << output;
            EXPECT_EQ(output.find("ended_at"), std::string::npos) << output;
            EXPECT_EQ(output.find(std::to_string(kStart.value)), std::string::npos)
                    << "the run's date belongs in the database, not in a fixture";
        }

        TEST_F(SimulateTest, TheOutputCarriesEveryFieldOfTheDocumentedSchema) {
            const std::string output = run(script_from(script_of("abc")));

            for (const std::string_view field:
                 {"schema", "mode", "mode_param", "provider", "provider_seed", "completed", "duration_ms",
                  "graphemes_typed", "graphemes_correct", "errors_total", "errors_uncorrected", "backspaces", "raw_wpm",
                  "gross_wpm", "net_wpm", "accuracy", "final_correctness", "consistency", "timeline"}) {
                EXPECT_NE(output.find(std::string{"\""} + std::string{field} + "\":"), std::string::npos)
                        << field << " is missing from " << output;
            }
            EXPECT_TRUE(output.starts_with(R"({"schema":1,)")) << output;
            EXPECT_TRUE(output.ends_with("]}")) << output;
        }

        TEST_F(SimulateTest, TheTimelineIsOneSamplePerSecondOfTheRun) {
            const std::string output = run(script_from(script_of(kAlphabet.substr(0, 25))));

            ASSERT_EQ(history_.records.size(), 1U);
            // A 24-second log, in one-second buckets, with the last event
            // counted in the bucket it closes.
            EXPECT_EQ(history_.records.front().timeline.size(), 24U);
            EXPECT_NE(output.find(R"("timeline":[{"at_ms":0,)"), std::string::npos) << output;
        }

        TEST_F(SimulateTest, MistakesAndCorrectionsReachTheRecord) {
            const Script script = script_from(
                    "# typeit-script 1\n"
                    "1000  type a\n"
                    "2000  type x\n"
                    "3000  backspace\n"
                    "4000  type b\n"
                    "5000  type z\n");

            const std::string output = run(script);

            EXPECT_NE(output.find(R"("backspaces":1)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("errors_total":2)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("errors_uncorrected":1)"), std::string::npos)
                    << "the corrected one no longer stands; the last one does";
        }

        TEST_F(SimulateTest, AMultiByteGraphemeIsOneKeystrokeAllTheWayThrough) {
            const Script script = script_from(
                    "# typeit-script 1\n"
                    "1000  type č\n"
                    "2000  type š\n");

            const std::string output = run(script, a_request("quote", "čš"));

            EXPECT_NE(output.find(R"("graphemes_typed":2)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("graphemes_correct":2)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("completed":true)"), std::string::npos) << output;
        }

        TEST_F(SimulateTest, ASpaceCrossesAWordBoundaryLikeAnyOtherKeystroke) {
            const std::string output = run(script_from(script_of("ab cd")), a_request("quote", "ab cd"));

            EXPECT_NE(output.find(R"("graphemes_typed":5)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("completed":true)"), std::string::npos) << output;
        }

        TEST_F(SimulateTest, AScriptWithNoEventsIsARunNobodyTypedIn) {
            const std::string output = run(script_from("# typeit-script 1\n"));

            EXPECT_NE(output.find(R"("graphemes_typed":0)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("duration_ms":0)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("timeline":[])"), std::string::npos) << output;
            // Zeros rather than NaN: a run with nothing in it is a normal thing
            // to report on.
            EXPECT_NE(output.find(R"("net_wpm":0)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("accuracy":0)"), std::string::npos) << output;
        }

        TEST_F(SimulateTest, AFailureToStartIsReportedRatherThanSimulated) {
            app::SessionRequest request = a_request();
            request.mode = "telepathy";

            const core::Result<std::string> output = simulate(service_, request, script_from(script_of("abc")));

            ASSERT_FALSE(output);
            EXPECT_EQ(output.error().code, core::ErrorCode::UnknownMode);
            EXPECT_TRUE(history_.records.empty());
        }

        TEST_F(SimulateTest, AFailureToSaveIsReportedRatherThanPrinted) {
            // Printing metrics for a run that was not saved would be a lie the
            // next `--stats` would contradict.
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the disk is on fire"));

            const core::Result<std::string> output = simulate(service_, a_request(), script_from(script_of("abc")));

            ASSERT_FALSE(output);
            EXPECT_EQ(output.error().code, core::ErrorCode::DbQuery);
        }

        TEST_F(SimulateTest, TheProviderSeedIsCarriedIntoTheOutput) {
            // What turns a bug report into a deterministic repro.
            const std::string output = run(script_from(script_of("abc")));

            EXPECT_NE(output.find(R"("provider_seed":4242)"), std::string::npos) << output;
            EXPECT_NE(output.find(R"("provider":"whole")"), std::string::npos) << output;
        }

    }  // namespace
}  // namespace typeit::cli
