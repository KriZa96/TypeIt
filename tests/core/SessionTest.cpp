// The wiring, not the parts. Every piece a Session holds has its own tests;
// what is only testable here is that they stay in step — that the mode hears
// about the keystrokes that happened and no others, and that the text the
// model points at outlives the run.

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string_view>
#include <utility>

#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/session/Session.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text_supply/ChunkedProvider.h"
#include "typeit/core/text_supply/WholeTextProvider.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/SpyMode.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::core {
    namespace {

        /// A session over `text`, with a mode the test keeps a handle on.
        struct Fixture {
            testing::SpyMode* mode = nullptr;
            std::unique_ptr<Session> session;
        };

        Fixture a_session(std::string_view text, TypingRules rules = {}, std::uint64_t seed = 0) {
            auto mode = std::make_unique<testing::SpyMode>();
            testing::SpyMode* observer = mode.get();
            Result<std::unique_ptr<Session>> session = Session::create(
                    Session::Parts{
                            .mode = std::move(mode),
                            .provider = std::make_unique<WholeTextProvider>(text, seed),
                            .rules = rules,
                    },
                    Millis{0});
            EXPECT_TRUE(session) << (session ? "" : session.error().message);

            Fixture fixture{.mode = observer, .session = nullptr};
            if (session) {
                fixture.session = std::move(*session);
            }
            return fixture;
        }

        /// The first grapheme of `text`, so a test can type one thing without
        /// segmenting it by hand.
        Grapheme key(std::string_view text) {
            const TextBuffer typed = testing::text_of(text);
            EXPECT_FALSE(typed.empty());
            return typed.at(GraphemeIndex{0});
        }

        TEST(SessionTest, TakesItsTextFromTheProvidersFirstChunk) {
            const Fixture run = a_session("hello");

            EXPECT_EQ(run.session->text().to_string(), "hello");
            EXPECT_EQ(run.session->model().size(), 5U);
        }

        TEST(SessionTest, StartsTheModeBeforeAnythingIsTyped) {
            // A mode over a fixed text has to be able to see the text before
            // the first keystroke — it may already be finished.
            const Fixture run = a_session("hi");

            ASSERT_FALSE(run.mode->calls.empty());
            EXPECT_EQ(run.mode->calls.front(), "on_start");
            EXPECT_EQ(run.mode->started_at, Millis{0});
        }

        TEST(SessionTest, CarriesTheStartTimeItWasGiven) {
            auto mode = std::make_unique<testing::SpyMode>();
            Result<std::unique_ptr<Session>> session =
                    Session::create(Session::Parts{.mode = std::move(mode),
                                                   .provider = std::make_unique<WholeTextProvider>("hi"),
                                                   .rules = {}},
                                    Millis{5'000});

            ASSERT_TRUE(session);
            EXPECT_EQ((*session)->started_at(), Millis{5'000});
        }

        TEST(SessionTest, AKeystrokeReachesBothTheModelAndTheMode) {
            const Fixture run = a_session("ab");

            run.session->on_key(key("a"), Millis{100});

            EXPECT_EQ(run.session->model().cursor(), GraphemeIndex{1});
            EXPECT_EQ(run.mode->keystrokes, 1U);
            EXPECT_EQ(run.mode->last_event_at, Millis{100});
        }

        TEST(SessionTest, AKeystrokeTheRulesRefusedIsNotReportedToTheMode) {
            // `stop_on_error` refuses input while an error stands. The
            // keystroke never reached a position, so counting it would charge
            // the typist for a key the game ignored — and a word-count mode
            // told about it would credit a word nobody typed.
            const Fixture run = a_session("ab", TypingRules{.stop_on_error = StopOnError::Letter});

            run.session->on_key(key("x"), Millis{100});
            ASSERT_EQ(run.mode->keystrokes, 1U) << "the mistake itself is a real keystroke";

            run.session->on_key(key("b"), Millis{200});

            EXPECT_EQ(run.mode->keystrokes, 1U);
            EXPECT_EQ(run.session->model().log().size(), 1U);
        }

        TEST(SessionTest, TypingPastTheEndIsNotAnEvent) {
            const Fixture run = a_session("a");

            run.session->on_key(key("a"), Millis{100});
            run.session->on_key(key("b"), Millis{200});

            EXPECT_EQ(run.mode->keystrokes, 1U);
            EXPECT_EQ(run.session->model().log().size(), 1U);
        }

        TEST(SessionTest, ABackspaceIsReportedButOnlyWhenThereIsSomethingToDelete) {
            const Fixture run = a_session("ab");

            run.session->on_backspace(Millis{100});
            EXPECT_EQ(run.mode->keystrokes, 0U) << "there is no position before the first one";

            run.session->on_key(key("a"), Millis{200});
            run.session->on_backspace(Millis{300});

            EXPECT_EQ(run.mode->keystrokes, 2U);
            EXPECT_EQ(run.session->model().cursor(), GraphemeIndex{0});
        }

        TEST(SessionTest, TicksReachTheModeWithNothingTyped) {
            // The reason a timed run can end while the typist stares at the
            // screen.
            const Fixture run = a_session("ab");

            run.session->on_tick(Millis{1'000});
            run.session->on_tick(Millis{2'000});

            EXPECT_EQ(run.mode->ticks, 2U);
            EXPECT_EQ(run.mode->last_tick, Millis{2'000});
            EXPECT_TRUE(run.session->model().log().empty());
        }

        TEST(SessionTest, TheModeDecidesWhenTheRunIsOver) {
            const Fixture run = a_session("ab");
            ASSERT_FALSE(run.session->is_finished());

            run.mode->finish();

            EXPECT_TRUE(run.session->is_finished());
        }

        TEST(SessionTest, AnEmptyTextIsARunThatIsAlreadyAtItsEnd) {
            // Not an error here: a run with nothing to type is rejected above
            // this layer, with a message this one could not write.
            auto mode = std::make_unique<QuoteMode>();
            const QuoteMode* observer = mode.get();
            Result<std::unique_ptr<Session>> session = Session::create(
                    Session::Parts{
                            .mode = std::move(mode), .provider = std::make_unique<WholeTextProvider>(""), .rules = {}},
                    Millis{0});

            ASSERT_TRUE(session);
            EXPECT_TRUE((*session)->model().at_end());
            EXPECT_TRUE(observer->is_finished());
        }

        TEST(SessionTest, InvalidUtf8FromTheProviderIsRefusedRatherThanSubstituted) {
            Result<std::unique_ptr<Session>> session =
                    Session::create(Session::Parts{.mode = std::make_unique<testing::SpyMode>(),
                                                   .provider = std::make_unique<WholeTextProvider>("ok\xffno"),
                                                   .rules = {}},
                                    Millis{0});

            ASSERT_FALSE(session);
            EXPECT_EQ(session.error().code, ErrorCode::InvalidUtf8);
        }

        TEST(SessionTest, KeepsTheProviderSoTheRunCanBeReproduced) {
            // The seed is what turns a bug report into a deterministic repro,
            // and `app` records it off the session rather than off whatever the
            // caller happened to still be holding.
            const Fixture run = a_session("ab", {}, 4'242);

            EXPECT_EQ(run.session->provider().seed(), 4'242U);
        }

        TEST(SessionTest, TakesOneChunkAndLeavesTheRestForTheNextSitting) {
            // A chunked text is typed over several sessions. Refilling mid-run
            // would move the text out from under the model's span, which is
            // why the session takes exactly one chunk.
            Result<std::unique_ptr<ChunkedProvider>> provider =
                    ChunkedProvider::create("one two three four", ChunkedProvider::Options{.chunk_graphemes = 7});
            ASSERT_TRUE(provider);

            Result<std::unique_ptr<Session>> session =
                    Session::create(Session::Parts{.mode = std::make_unique<testing::SpyMode>(),
                                                   .provider = std::move(*provider),
                                                   .rules = {}},
                                    Millis{0});

            ASSERT_TRUE(session);
            EXPECT_EQ((*session)->text().to_string(), "one ");
            EXPECT_TRUE((*session)->provider().has_more());
        }

        TEST(SessionTest, TwoSessionsShareNothing) {
            // The regression guard for the deleted globals: a run is an object,
            // so a second one starts empty however the first one went.
            const Fixture first = a_session("ab");
            first.session->on_key(key("a"), Millis{100});
            first.session->on_key(key("b"), Millis{200});

            const Fixture second = a_session("ab");

            EXPECT_TRUE(second.session->model().log().empty());
            EXPECT_EQ(second.session->model().cursor(), GraphemeIndex{0});
            EXPECT_EQ(second.mode->keystrokes, 0U);
        }

    }  // namespace
}  // namespace typeit::core
