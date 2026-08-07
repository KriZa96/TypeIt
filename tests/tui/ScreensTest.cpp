// The five screens.
//
// One file rather than five: they share a fixture, and the interesting
// assertions are about how they behave rather than how they look. The two that
// are about looking have goldens.

#include <cstddef>
#include <filesystem>
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ScreenStack.h"
#include "Snapshot.h"
#include "screens/HelpScreen.h"
#include "screens/MenuScreen.h"
#include "screens/ResultsScreen.h"
#include "screens/SessionScreen.h"
#include "screens/TerminalTooSmallScreen.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/core/modes/ModeRegistry.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/modes/TimedMode.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::tui {
    namespace {

        /// Everything a screen borrows, owned for the length of a test.
        struct Fixture {
            app::Theme theme;
            Keymap keymap;
            core::Config config;
            /// The three corpora the application ships, without the disk they
            /// normally live on — the loader below is a parameter precisely so
            /// a test can be the file system.
            std::vector<TextChoice> texts{{.name = "simple", .path = "simple.txt"},
                                          {.name = "medium", .path = "medium.txt"},
                                          {.name = "hard", .path = "hard.txt"}};
            ScreenContext context;

            Fixture() {
                config.general.countdown_s = 0;  // Most tests are not about the countdown.
                context.theme = &theme;
                context.keymap = &keymap;
                context.config = &config;
                context.texts = &texts;
                context.load_text = [](const std::filesystem::path& path) -> core::Result<std::string> {
                    if (path.extension() == ".txt") {
                        return std::string{"the quick brown fox"};
                    }
                    return core::fail(core::ErrorCode::FileNotFound, path.string());
                };
                context.size = TerminalSize{.columns = 80, .rows = 24};
            }
        };

        /// A session service over fakes, plus the registry it resolves modes
        /// through. Kept together because the service borrows both.
        struct Runner {
            testing::FakeClock clock{core::Millis{1'767'225'600'000}};
            testing::FakeHistoryRepository history;
            core::ModeRegistry modes;
            app::SessionService service{history, modes, clock, clock};

            Runner() {
                modes.register_mode("quote", [] { return std::make_unique<core::QuoteMode>(); });
                modes.register_mode("timed", [] { return std::make_unique<core::TimedMode>(core::Millis{30'000}); });
            }

            [[nodiscard]] static app::SessionRequest a_request(std::string_view mode = "quote",
                                                               std::string_view text = "hi there") {
                app::SessionRequest request;
                request.mode = mode;
                request.mode_param = R"({"seconds":30})";
                request.text = text;
                return request;
            }
        };

        // --- TerminalTooSmallScreen -------------------------------------------

        TEST(TerminalTooSmallTest, ItAppearsBelowTheMinimumAndGoesAwayAboveIt) {
            TooSmallGate gate;

            EXPECT_FALSE(gate.update({.columns = 80, .rows = 24}));
            EXPECT_TRUE(gate.update({.columns = 30, .rows = 24}));
            EXPECT_FALSE(gate.update({.columns = 80, .rows = 24}));
        }

        TEST(TerminalTooSmallTest, ATerminalOnTheBoundaryDoesNotFlicker) {
            // Without hysteresis a drag that wobbles by one row would toggle
            // between two screens on every frame, which is worse than either.
            TooSmallGate gate;
            ASSERT_TRUE(gate.update({.columns = 39, .rows = 24}));

            // Exactly on the minimum: still showing, because going away needs
            // room to spare.
            EXPECT_TRUE(gate.update(kMinimumSize));
            EXPECT_TRUE(gate.update({.columns = kMinimumSize.columns + 1, .rows = kMinimumSize.rows + 1}));

            EXPECT_FALSE(gate.update({.columns = kMinimumSize.columns + kTooSmallHysteresis,
                                      .rows = kMinimumSize.rows + kTooSmallHysteresis}));
        }

        TEST(TerminalTooSmallTest, ItReportsBothSizes) {
            // "Too small" without the numbers leaves somebody resizing by
            // guesswork.
            Fixture fixture;
            fixture.context.size = TerminalSize{.columns = 60, .rows = 20};
            TerminalTooSmallScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 60, 20);

            EXPECT_NE(drawn.find("60x20"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("40x10"), std::string::npos) << drawn;
        }

        TEST(TerminalTooSmallTest, QuittingStillWorksAndNothingElseIsSwallowed) {
            Fixture fixture;
            TerminalTooSmallScreen screen{fixture.context};

            EXPECT_TRUE(screen.on_event(ftxui::Event::Special(std::string(1, '\x11'))));  // ctrl-q
            EXPECT_FALSE(screen.on_event(ftxui::Event::Character('a')));
        }

        TEST(TerminalTooSmallTest, TheSessionUnderneathIsUntouched) {
            // It is pushed on top, never in place of, so a terminal shrunk
            // mid-run and grown back finds the run where it was.
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> session =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request(), runner.clock);
            ASSERT_TRUE(session);
            ASSERT_TRUE((*session)->on_event(ftxui::Event::Character("h")));

            // A non-owning handle: the session screen is owned by the
            // `Result` above, and the stack only has to be able to reach it.
            ScreenStack stack;
            stack.push(std::shared_ptr<IScreen>{session->get(), [](IScreen*) {}});
            stack.push(std::make_shared<TerminalTooSmallScreen>(fixture.context));
            stack.pop();

            EXPECT_EQ((*session)->session().model().cursor().value, 1U) << "the keystroke survived";
        }

        // --- HelpScreen ---------------------------------------------------------

        TEST(HelpScreenTest, ItListsTheActualBindings) {
            Fixture fixture;
            HelpScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find("ctrl-q"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("f1"), std::string::npos) << drawn;
        }

        TEST(HelpScreenTest, ItFollowsARebindRatherThanAHardcodedList) {
            // A help screen showing the defaults after a rebind is worse than
            // no help screen, because it is confidently wrong.
            Fixture fixture;
            fixture.keymap = Keymap::from_config({{"force_quit", "ctrl-x"}});
            HelpScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find("ctrl-x"), std::string::npos) << drawn;
            EXPECT_EQ(drawn.find("ctrl-q"), std::string::npos) << drawn;
        }

        TEST(HelpScreenTest, ItSaysThatFontSizeIsATerminalSetting) {
            // UX §1: the one thing people ask for that a terminal program
            // cannot give them, said once here rather than in a bug report
            // every month.
            Fixture fixture;
            HelpScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find("Font size"), std::string::npos) << drawn;
        }

        TEST(HelpScreenTest, ItScrollsWhenThereIsMoreThanFits) {
            Fixture fixture;
            fixture.context.size = TerminalSize{.columns = 80, .rows = 6};
            HelpScreen screen{fixture.context};

            const std::string first = testing::render_to_text(screen.render(), 80, 6);
            EXPECT_TRUE(screen.scrollable());

            EXPECT_TRUE(screen.on_event(ftxui::Event::ArrowDown));
            const std::string scrolled = testing::render_to_text(screen.render(), 80, 6);

            EXPECT_NE(first, scrolled);
            EXPECT_EQ(screen.scroll(), 1U);
        }

        TEST(HelpScreenTest, ScrollingUpStopsAtTheTop) {
            Fixture fixture;
            HelpScreen screen{fixture.context};

            EXPECT_TRUE(screen.on_event(ftxui::Event::ArrowUp));

            EXPECT_EQ(screen.scroll(), 0U);
        }

        // --- MenuScreen ----------------------------------------------------------

        TEST(MenuScreenTest, TheSelectionIsHeldByTheScreen) {
            // Not by a global, which is where 1.0 keeps it and why two menus
            // could never coexist.
            Fixture fixture;
            MenuScreen first{fixture.context};
            MenuScreen second{fixture.context};

            ASSERT_TRUE(first.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(first.on_event(ftxui::Event::Character('4')));

            EXPECT_EQ(first.selection().seconds, 4);
            EXPECT_EQ(second.selection().seconds, fixture.config.general.default_duration_s);
        }

        TEST(MenuScreenTest, ItStartsFromTheConfiguration) {
            Fixture fixture;
            fixture.config.general.default_duration_s = 45;
            fixture.config.general.default_mode = "zen";
            const MenuScreen screen{fixture.context};

            EXPECT_EQ(screen.selection().seconds, 45);
            EXPECT_EQ(screen.selection().mode, "zen");
        }

        TEST(MenuScreenTest, CustomInputIsValidatedOnChangeAndReportsTheRange) {
            // 1.0 parses inside a render callback with an empty catch block, so
            // a bad value is swallowed sixty times a second and nobody is told.
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));  // to seconds

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('0')));

            EXPECT_FALSE(screen.message().empty()) << "a zero-second run is not a run";
            EXPECT_NE(screen.message().find("3600"), std::string::npos)
                    << "and the message names the range: " << screen.message();
        }

        TEST(MenuScreenTest, TheMessageIsSetOnChangeRatherThanByRendering) {
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('0')));

            const std::string after_edit = screen.message();
            const std::string ignored = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_EQ(screen.message(), after_edit) << "rendering changed the message";
            EXPECT_FALSE(ignored.empty());
        }

        TEST(MenuScreenTest, AValidValueClearsTheMessage) {
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('0')));
            ASSERT_FALSE(screen.message().empty());

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('3')));  // 03 -> 3

            EXPECT_TRUE(screen.message().empty()) << screen.message();
            EXPECT_EQ(screen.selection().seconds, 3);
        }

        TEST(MenuScreenTest, StartingWithAnInvalidSelectionIsRefusedVisibly) {
            // Rather than silently doing nothing, which is what 1.0 does.
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('0')));

            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_FALSE(screen.take_start()) << "it did not start";
            EXPECT_FALSE(screen.message().empty()) << "and it said why";
        }

        TEST(MenuScreenTest, StartingWithAValidSelectionWorks) {
            Fixture fixture;
            MenuScreen screen{fixture.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_TRUE(screen.take_start());
            EXPECT_FALSE(screen.take_start()) << "and is read exactly once";
        }

        TEST(MenuScreenTest, TabOrderReachesEveryControlAndIsStable) {
            Fixture fixture;
            MenuScreen screen{fixture.context};

            std::vector<MenuField> visited;
            for (std::size_t step = 0; step < kMenuFields.size(); ++step) {
                visited.push_back(screen.focused());
                ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            }

            EXPECT_EQ(visited.size(), kMenuFields.size());
            EXPECT_EQ(screen.focused(), MenuField::Mode) << "and wraps back to the start";

            // Backwards reaches them too, in reverse.
            ASSERT_TRUE(screen.on_event(ftxui::Event::TabReverse));
            EXPECT_EQ(screen.focused(), MenuField::Start);
        }

        TEST(MenuScreenTest, TheModeCyclesWithTheArrows) {
            Fixture fixture;
            MenuScreen screen{fixture.context};
            const std::string first = screen.selection().mode;

            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));
            EXPECT_NE(screen.selection().mode, first);

            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowLeft));
            EXPECT_EQ(screen.selection().mode, first);
        }

        TEST(MenuScreenTest, TheBundledTextsCycleWithTheArrowsAndEndAtACustomPath) {
            // 1.0's parity item: three difficulty texts plus "type a path",
            // which is exactly what its fourth radio button was.
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_EQ(screen.focused(), MenuField::Text);

            EXPECT_EQ(screen.selection().text, 0U);
            for (std::size_t step = 1; step <= fixture.texts.size(); ++step) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));
                EXPECT_EQ(screen.selection().text, step);
            }
            // One past the last entry is the custom path, and one more wraps.
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));
            EXPECT_EQ(screen.selection().text, 0U);
        }

        TEST(MenuScreenTest, ACustomPathThatCannotBeReadIsReportedAndRefusesToStart) {
            // The validity feedback 1.0 never had: it opened whatever was typed
            // and showed an empty typing area when the open failed.
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            // One press per entry lands one past the last, which is the
            // custom path.
            for (std::size_t step = 0; step < fixture.texts.size(); ++step) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));
            }
            EXPECT_FALSE(screen.message().empty()) << "an empty path is not a path";

            for (const char letter: std::string_view{"nope.md"}) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::Character(letter)));
            }
            EXPECT_EQ(screen.selection().custom_path, "nope.md");
            EXPECT_FALSE(screen.message().empty());

            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));
            EXPECT_FALSE(screen.take_start()) << "refused, with the reason still on screen";
        }

        TEST(MenuScreenTest, ACustomPathThatReadsClearsTheMessageAndStarts) {
            Fixture fixture;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            // One press per entry lands one past the last, which is the
            // custom path.
            for (std::size_t step = 0; step < fixture.texts.size(); ++step) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));
            }
            for (const char letter: std::string_view{"mine.txt"}) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::Character(letter)));
            }

            EXPECT_TRUE(screen.message().empty()) << screen.message();
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));
            EXPECT_TRUE(screen.take_start());

            // And backspace walks it back to being wrong again.
            ASSERT_TRUE(screen.on_event(ftxui::Event::Backspace));
            EXPECT_EQ(screen.selection().custom_path, "mine.tx");
            EXPECT_FALSE(screen.message().empty());
        }

        TEST(MenuScreenTest, WithNoCatalogueThereIsNothingToChooseAndStartingStillWorks) {
            // An installation missing its assets. The field says "built-in" and
            // the run types whatever the composition root supplied, rather than
            // offering three entries that fail the moment they are chosen.
            Fixture fixture;
            fixture.context.texts = nullptr;
            MenuScreen screen{fixture.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_EQ(screen.focused(), MenuField::Text);

            EXPECT_FALSE(screen.on_event(ftxui::Event::ArrowRight)) << "nothing to cycle through";
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));
            EXPECT_TRUE(screen.take_start());
        }

        // --- The menu's recent-runs sparkline (TI-106) --------------------------

        /// A history with `count` runs in it, oldest slowest, so a trend that
        /// came back in the wrong order is visible rather than merely wrong.
        void seed_runs(testing::FakeHistoryRepository& history, std::size_t count) {
            for (std::size_t at = 0; at < count; ++at) {
                app::SessionRecord record;
                record.mode = "timed";
                record.started_at = core::Millis{1'767'225'600'000 + static_cast<std::int64_t>(at) * 60'000};
                record.ended_at = record.started_at;
                record.duration = core::Millis{30'000};
                record.net_wpm = core::Wpm{50.0 + static_cast<double>(at)};
                record.accuracy = core::Accuracy{0.9};
                record.completed = true;
                EXPECT_TRUE(history.save_run(record, {}, {}));
            }
        }

        TEST(MenuScreenTest, WithNoRunsTheSparklineIsAPromptRatherThanAnEmptyBox) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            fixture.context.history = HistorySource{.records = &history};
            MenuScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("no runs yet"), std::string::npos) << drawn;
        }

        TEST(MenuScreenTest, FewerThanTenRunsStillDraws) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            seed_runs(history, 3);
            fixture.context.history = HistorySource{.records = &history};
            MenuScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("last 3"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("avg"), std::string::npos) << drawn;
        }

        TEST(MenuScreenTest, TheFiguresBesideTheSparklineMatchTheRuns) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            seed_runs(history, 5);  // 50, 51, 52, 53, 54 wpm at 90%.
            fixture.context.history = HistorySource{.records = &history};
            MenuScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("avg 52"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("best 54"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("acc 90%"), std::string::npos) << drawn;
        }

        TEST(MenuScreenTest, MoreThanTenRunsShowsTheLastTen) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            seed_runs(history, 25);
            fixture.context.history = HistorySource{.records = &history};
            MenuScreen screen{fixture.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("last 10"), std::string::npos) << drawn;
            // The last ten are the fastest ten, so the average is well above
            // the lifetime one — which is the check that it took the *recent*
            // runs rather than the first ten it found.
            EXPECT_NE(drawn.find("best 74"), std::string::npos) << drawn;
        }

        TEST(MenuScreenTest, TheHistoryIsReadOnceRatherThanOnEveryFrame) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            seed_runs(history, 5);
            fixture.context.history = HistorySource{.records = &history};
            MenuScreen screen{fixture.context};
            const std::size_t after_open = history.queries;

            static_cast<void>(testing::render_to_text(screen.render(), 80, 24));
            static_cast<void>(testing::render_to_text(screen.render(), 80, 24));

            EXPECT_EQ(history.queries, after_open) << "drawing asked the database nothing";
        }

        TEST(MenuScreenTest, AFailingHistoryQueryLeavesTheMenuUsable) {
            // A run nobody can start because the sparkline failed to load would
            // be the worse trade.
            Fixture fixture;
            testing::FakeHistoryRepository history;
            history.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the fixture asked it to"));
            fixture.context.history = HistorySource{.records = &history};
            MenuScreen screen{fixture.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));
            EXPECT_TRUE(screen.take_start());
        }

        TEST(MenuScreenTest, SnapshotAt80x24) {
            Fixture fixture;
            MenuScreen screen{fixture.context};

            testing::expect_matches_golden("menu_80x24", testing::render_to_text(screen.render(), 80, 24));
        }

        // --- SessionScreen --------------------------------------------------------

        TEST(SessionScreenTest, KeysAdvanceTheModelAndTicksAdvanceTheMode) {
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> screen =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request("timed"), runner.clock);
            ASSERT_TRUE(screen);

            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));
            EXPECT_EQ((*screen)->session().model().cursor().value, 1U) << "the key reached the model";

            const std::size_t events = (*screen)->session().model().log().size();
            runner.clock.advance(core::Millis{1'000});
            (*screen)->on_tick(runner.clock.now());

            EXPECT_EQ((*screen)->session().model().log().size(), events) << "the tick typed nothing";
        }

        TEST(SessionScreenTest, RenderingAdvancesNeither) {
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> screen =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request("timed"), runner.clock);
            ASSERT_TRUE(screen);
            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));

            const std::size_t events = (*screen)->session().model().log().size();
            const std::string first = testing::render_to_text((*screen)->render(), 80, 24);
            const std::string second = testing::render_to_text((*screen)->render(), 80, 24);

            EXPECT_EQ(first, second);
            EXPECT_EQ((*screen)->session().model().log().size(), events);
        }

        TEST(SessionScreenTest, TheCountdownDelaysTheStartAndTheTimerStillBeginsOnTheFirstKeystroke) {
            Fixture fixture;
            fixture.config.general.countdown_s = 3;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> screen =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request("timed"), runner.clock);
            ASSERT_TRUE(screen);

            EXPECT_EQ((*screen)->countdown(), 3);
            // A keystroke during the countdown is aimed at a screen that is not
            // ready for it, and is dropped rather than queued.
            EXPECT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));
            EXPECT_TRUE((*screen)->session().model().log().empty());

            runner.clock.advance(core::Millis{3'000});
            (*screen)->on_tick(runner.clock.now());
            EXPECT_EQ((*screen)->countdown(), 0);

            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));
            EXPECT_EQ((*screen)->session().model().log().size(), 1U);
        }

        TEST(SessionScreenTest, ModeCompletionSavesExactlyOnce) {
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> screen = SessionScreen::create(
                    fixture.context, runner.service, Runner::a_request("quote", "hi"), runner.clock);
            ASSERT_TRUE(screen);

            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));
            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("i")));

            (*screen)->on_tick(runner.clock.now());
            (*screen)->on_tick(runner.clock.now());
            (*screen)->on_tick(runner.clock.now());

            EXPECT_EQ(runner.history.saves, 1U) << "a mode that stays finished must not save again";
            EXPECT_TRUE((*screen)->result().has_value());
            EXPECT_EQ((*screen)->take_outcome(), SessionOutcome::Finished);
        }

        TEST(SessionScreenTest, QuittingMidRunRecordsAnAbandonedSession) {
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> screen =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request(), runner.clock);
            ASSERT_TRUE(screen);
            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));

            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Escape));

            ASSERT_EQ(runner.history.records.size(), 1U) << "it happened, so it is history";
            EXPECT_FALSE(runner.history.records.front().completed);
            EXPECT_EQ((*screen)->take_outcome(), SessionOutcome::Abandoned);
        }

        TEST(SessionScreenTest, RestartAsksForAFreshRunAndSavesTheOldOne) {
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> screen =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request(), runner.clock);
            ASSERT_TRUE(screen);
            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Character("h")));

            ASSERT_TRUE((*screen)->on_event(ftxui::Event::Special(std::string(1, '\x12'))));  // ctrl-r

            EXPECT_EQ((*screen)->take_outcome(), SessionOutcome::Restart);
            EXPECT_EQ(runner.history.records.size(), 1U) << "a restart is not a reason to pretend it did not happen";
        }

        TEST(SessionScreenTest, AFreshSessionCarriesNothingOver) {
            // The regression guard for the globals: two runs in one process
            // must share nothing.
            Fixture fixture;
            Runner runner;
            const core::Result<std::unique_ptr<SessionScreen>> first =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request(), runner.clock);
            ASSERT_TRUE(first);
            runner.clock.advance(core::Millis{100});
            ASSERT_TRUE((*first)->on_event(ftxui::Event::Character("h")));

            const core::Result<std::unique_ptr<SessionScreen>> second =
                    SessionScreen::create(fixture.context, runner.service, Runner::a_request(), runner.clock);
            ASSERT_TRUE(second);

            EXPECT_TRUE((*second)->session().model().log().empty());
            EXPECT_EQ((*second)->session().model().cursor().value, 0U);
        }

        TEST(SessionScreenTest, AnUnstartableRunIsReportedRatherThanDrawn) {
            Fixture fixture;
            Runner runner;

            const core::Result<std::unique_ptr<SessionScreen>> screen = SessionScreen::create(
                    fixture.context, runner.service, Runner::a_request("telepathy"), runner.clock);

            ASSERT_FALSE(screen);
            EXPECT_EQ(screen.error().code, core::ErrorCode::UnknownMode);
        }

        // --- ResultsScreen ----------------------------------------------------------

        app::SessionRecord a_record() {
            app::SessionRecord record;
            record.mode = "timed";
            record.net_wpm = core::Wpm{72.5};
            record.gross_wpm = core::Wpm{80.0};
            record.raw_wpm = core::Wpm{82.0};
            record.accuracy = core::Accuracy{0.97};
            record.final_correctness = core::Accuracy{1.0};
            record.consistency = 88.0;
            record.graphemes_typed = 300;
            record.errors_total = 9;
            record.duration = core::Millis{30'000};
            record.completed = true;
            return record;
        }

        TEST(ResultsScreenTest, EveryMetricRendersWithItsUnits) {
            Fixture fixture;
            ResultsScreen screen{fixture.context, a_record()};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("72.5"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("97%"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("300"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("30"), std::string::npos) << drawn;
        }

        TEST(ResultsScreenTest, AZeroKeystrokeSessionRendersWithoutNaNOrACrash) {
            // Everything divides by something that can be zero.
            Fixture fixture;
            ResultsScreen screen{fixture.context, app::SessionRecord{}};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_FALSE(drawn.empty());
            EXPECT_EQ(drawn.find("nan"), std::string::npos) << drawn;
            EXPECT_EQ(drawn.find("inf"), std::string::npos) << drawn;
        }

        TEST(ResultsScreenTest, AnAbandonedRunSaysSo) {
            Fixture fixture;
            app::SessionRecord abandoned = a_record();
            abandoned.completed = false;
            ResultsScreen screen{fixture.context, abandoned};

            EXPECT_NE(testing::render_to_text(screen.render(), 80, 24).find("abandoned"), std::string::npos);
        }

        TEST(ResultsScreenTest, EachActionIsReportedExactlyOnce) {
            Fixture fixture;
            ResultsScreen screen{fixture.context, a_record()};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Special(std::string(1, '\x12'))));  // ctrl-r
            EXPECT_EQ(screen.take_action(), ResultsAction::Restart);
            EXPECT_FALSE(screen.take_action().has_value());

            ASSERT_TRUE(screen.on_event(ftxui::Event::Special(std::string(1, '\x0e'))));  // ctrl-n
            EXPECT_EQ(screen.take_action(), ResultsAction::NewText);

            ASSERT_TRUE(screen.on_event(ftxui::Event::Escape));
            EXPECT_EQ(screen.take_action(), ResultsAction::Menu);
        }

        // --- ResultsScreen enrichment (TI-105) ----------------------------------

        /// A finished run with everything `finish` produces: the record, the
        /// per-second samples, the pairs got wrong and the bigram latencies.
        app::SessionResult a_result() {
            app::SessionResult result;
            result.id = core::SessionId{1};
            result.record = a_record();
            for (std::int64_t second = 0; second < 8; ++second) {
                result.record.timeline.push_back({.at = core::Millis{second * 1'000},
                                                  .wpm = core::Wpm{60.0 + static_cast<double>(second)},
                                                  .keystrokes = 6,
                                                  .errors = second == 2 ? 1U : 0U});
            }
            result.errors.substitutions[{"m", "n"}] = 7;
            result.errors.substitutions[{"e", "r"}] = 3;
            result.keys.per_bigram["th"] = {
                    .attempts = 20, .errors = 0, .total_latency = core::Millis{4'000}, .latency_samples = 20};
            result.keys.per_bigram["qu"] = {
                    .attempts = 4, .errors = 0, .total_latency = core::Millis{1'600}, .latency_samples = 4};
            // Measured nothing, so it is unmeasured rather than instant.
            result.keys.per_bigram["zz"] = {
                    .attempts = 2, .errors = 0, .total_latency = core::Millis{0}, .latency_samples = 0};
            return result;
        }

        TEST(ResultsScreenTest, TheChartMatchesTheSessionTimeline) {
            Fixture fixture;
            ResultsScreen screen{fixture.context, a_result()};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find('*'), std::string::npos) << "the per-second series:\n" << drawn;
            EXPECT_NE(drawn.find('x'), std::string::npos) << "and the second an error happened in:\n" << drawn;
        }

        TEST(ResultsScreenTest, WithNoHistoryTheComparisonIsAbsentRatherThanZero) {
            // A line reading "+0 vs average" on somebody's first run invents a
            // baseline out of the run itself.
            Fixture fixture;
            ResultsScreen screen{fixture.context, a_result()};

            EXPECT_FALSE(screen.comparison().baseline.has_value());
            EXPECT_EQ(testing::render_to_text(screen.render(), 80, 40).find("vs your average"), std::string::npos);
        }

        TEST(ResultsScreenTest, WithHistoryTheComparisonIsShownAgainstAverageAndBest) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            // Two earlier runs at 60 and 80, so the average is 70 and the best
            // is 80 — and this run's 72.5 sits between them.
            for (const double wpm: {60.0, 80.0}) {
                app::SessionRecord past = a_record();
                past.net_wpm = core::Wpm{wpm};
                EXPECT_TRUE(history.save_run(past, {}, {}));
            }
            app::SessionRecord mine = a_record();
            EXPECT_TRUE(history.save_run(mine, {}, {}));
            fixture.context.history = HistorySource{.records = &history};
            ResultsScreen screen{fixture.context, a_result()};

            ASSERT_TRUE(screen.comparison().baseline.has_value());
            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);
            EXPECT_NE(drawn.find("vs your average"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("vs your best"), std::string::npos) << drawn;
        }

        TEST(ResultsScreenTest, ANewPersonalBestIsAnnouncedDistinctly) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            app::SessionRecord slower = a_record();
            slower.net_wpm = core::Wpm{50.0};
            EXPECT_TRUE(history.save_run(slower, {}, {}));
            EXPECT_TRUE(history.save_run(a_record(), {}, {}));  // 72.5, the new best.
            fixture.context.history = HistorySource{.records = &history};
            ResultsScreen screen{fixture.context, a_result()};

            EXPECT_TRUE(screen.comparison().personal_best);
            EXPECT_NE(testing::render_to_text(screen.render(), 80, 40).find("personal best"), std::string::npos);
        }

        TEST(ResultsScreenTest, ARunThatIsNotABestSaysNothingAboutOne) {
            Fixture fixture;
            testing::FakeHistoryRepository history;
            app::SessionRecord faster = a_record();
            faster.net_wpm = core::Wpm{200.0};
            EXPECT_TRUE(history.save_run(faster, {}, {}));
            EXPECT_TRUE(history.save_run(a_record(), {}, {}));
            fixture.context.history = HistorySource{.records = &history};
            ResultsScreen screen{fixture.context, a_result()};

            EXPECT_FALSE(screen.comparison().personal_best);
            EXPECT_EQ(testing::render_to_text(screen.render(), 80, 40).find("personal best"), std::string::npos);
        }

        TEST(ResultsScreenTest, TheWorstPairsAndSlowestBigramsAreListedBiggestFirst) {
            Fixture fixture;
            ResultsScreen screen{fixture.context, a_result()};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find("missed"), std::string::npos) << drawn;
            const std::size_t worst = drawn.find("m->n");
            const std::size_t next = drawn.find("e->r");
            ASSERT_NE(worst, std::string::npos) << drawn;
            ASSERT_NE(next, std::string::npos) << drawn;
            EXPECT_LT(worst, next) << "seven beats three:\n" << drawn;

            EXPECT_NE(drawn.find("slowest"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("qu 400ms"), std::string::npos) << drawn;
            EXPECT_EQ(drawn.find("zz"), std::string::npos) << "unmeasured is not slow:\n" << drawn;
        }

        TEST(ResultsScreenTest, SparseDataShowsFewerEntriesRatherThanPadding) {
            Fixture fixture;
            app::SessionResult sparse = a_result();
            sparse.errors.substitutions.clear();
            sparse.errors.substitutions[{"a", "s"}] = 1;
            ResultsScreen screen{fixture.context, sparse};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find("a->s 1"), std::string::npos) << drawn;
        }

        TEST(ResultsScreenTest, APerfectRunListsNoMistakesAtAll) {
            Fixture fixture;
            app::SessionResult clean = a_result();
            clean.errors.substitutions.clear();
            ResultsScreen screen{fixture.context, clean};

            EXPECT_EQ(testing::render_to_text(screen.render(), 80, 40).find("missed"), std::string::npos);
        }

        TEST(ResultsScreenTest, MultiByteGraphemesSurviveThePairList) {
            // The one place a `ć` is most likely to appear, because it is the
            // one people miss — and 1.0's bug was reading one byte of it.
            Fixture fixture;
            app::SessionResult accented = a_result();
            accented.errors.substitutions.clear();
            accented.errors.substitutions[{"ć", "c"}] = 4;
            ResultsScreen screen{fixture.context, accented};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 40);

            EXPECT_NE(drawn.find("ć"), std::string::npos) << drawn;
        }

        TEST(ResultsScreenTest, SnapshotAt80x24) {
            Fixture fixture;
            ResultsScreen screen{fixture.context, a_record()};

            testing::expect_matches_golden("results_80x24", testing::render_to_text(screen.render(), 80, 24));
        }

        TEST(ResultsScreenTest, SnapshotOfTheEnrichedScreen) {
            // The parity snapshot above is the record-only constructor, which
            // is still what a run started before there was any history to
            // compare against produces. This is the whole screen.
            Fixture fixture;
            testing::FakeHistoryRepository history;
            app::SessionRecord slower = a_record();
            slower.net_wpm = core::Wpm{60.0};
            EXPECT_TRUE(history.save_run(slower, {}, {}));
            EXPECT_TRUE(history.save_run(a_record(), {}, {}));
            fixture.context.history = HistorySource{.records = &history};
            ResultsScreen screen{fixture.context, a_result()};

            testing::expect_matches_golden("results_enriched_80x40", testing::render_to_text(screen.render(), 80, 40));
        }

        // --- The purity guard, on every screen -----------------------------------

        TEST(ScreenPurityTest, NoScreenChangesAnythingByDrawing) {
            // Phase 4's exit criterion, and the defect the whole rewrite is
            // about: 1.0 advances the thing it is measuring from inside a
            // render transform. `SessionScreen` has its own version of this
            // with the model asserted as well, because it is the one that owns
            // a model to get wrong; these four own only what they draw.
            Fixture fixture;
            MenuScreen menu{fixture.context};
            HelpScreen help{fixture.context};
            TerminalTooSmallScreen small{fixture.context};
            ResultsScreen results{fixture.context, a_record()};

            testing::expect_render_is_pure([&menu] { return menu.render(); }, 80, 24);
            testing::expect_render_is_pure([&help] { return help.render(); }, 80, 40);
            testing::expect_render_is_pure([&small] { return small.render(); }, 30, 10);
            testing::expect_render_is_pure([&results] { return results.render(); }, 80, 24);

            // And the menu still says what it said: a screen that reset its own
            // selection on the third draw would pass the comparison above and
            // still be wrong.
            EXPECT_EQ(menu.focused(), MenuField::Mode);
            EXPECT_EQ(menu.selection().text, 0U);
        }

    }  // namespace
}  // namespace typeit::tui
