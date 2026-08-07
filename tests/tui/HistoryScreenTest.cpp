// Everything recorded, on one screen (TI-103).
//
// Driven over the fake repository rather than a database: what is under test is
// which questions the screen asks and what it does with the answers, and a real
// SQLite file would only make that slower to say.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Snapshot.h"
#include "screens/HistoryScreen.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::tui {
    namespace {

        /// Noon on 1 January 2026, the instant every history test in this
        /// project is anchored to.
        constexpr core::Millis kToday{1'767'225'600'000};
        constexpr std::int64_t kMillisPerDay = 86'400'000;

        /// A history, a service over it, and the context a screen reads it
        /// through. Held together because the screen borrows all three.
        struct World {
            app::Theme theme;
            Keymap keymap;
            core::Config config;
            testing::FakeClock clock{kToday};
            testing::FakeHistoryRepository history;
            app::HistoryService service{history};
            ScreenContext context;

            /// Where an export lands, and what it was given. A closure rather
            /// than a real file: what is under test is what the screen asks
            /// for, and a temporary directory would only make that slower to
            /// say. The failing case is armed by setting `refuse`.
            std::filesystem::path written_to;
            std::string written;
            bool refuse = false;

            World() {
                context.save_text = [this](const std::filesystem::path& path,
                                           const std::string& contents) -> core::Status {
                    if (refuse) {
                        return core::fail(core::ErrorCode::FileUnreadable, path.string() + ": cannot write");
                    }
                    written_to = path;
                    written = contents;
                    return {};
                };
                context.theme = &theme;
                context.keymap = &keymap;
                context.config = &config;
                context.size = TerminalSize{.columns = 80, .rows = 24};
                context.history =
                        HistorySource{.service = &service, .records = &history, .wall_clock = &clock, .utc_offset = 0};
            }

            /// One run, `days_ago` days back, at `wpm`.
            void record(std::string mode, double wpm, std::int64_t days_ago, bool completed = true) {
                app::SessionRecord record;
                record.mode = std::move(mode);
                record.mode_param = R"({"seconds":30})";
                record.started_at = core::Millis{kToday.value - (days_ago * kMillisPerDay)};
                record.ended_at = record.started_at;
                record.duration = core::Millis{30'000};
                record.net_wpm = core::Wpm{wpm};
                record.gross_wpm = core::Wpm{wpm};
                record.accuracy = core::Accuracy{0.97};
                record.final_correctness = core::Accuracy{1.0};
                record.graphemes_typed = 150;
                record.completed = completed;
                EXPECT_TRUE(history.save_run(record, {}, {}));
            }
        };

        TEST(HistoryScreenTest, AnEmptyHistoryRendersAnInvitationRatherThanATableOfZeros) {
            // The state a new user opens this on, and the one most likely to
            // look like a bug: a screen of zeroes reads as broken, not as new.
            World world;
            HistoryScreen screen{world.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("Nothing recorded yet"), std::string::npos) << drawn;
            EXPECT_EQ(screen.data().totals.sessions, 0U);
        }

        TEST(HistoryScreenTest, NoHistorySourceAtAllStillRendersRatherThanCrashing) {
            // A layout test, or a build with the database unavailable. The
            // screen must not require one to exist.
            World world;
            world.context.history = {};
            HistoryScreen screen{world.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("Nothing recorded yet"), std::string::npos) << drawn;
        }

        TEST(HistoryScreenTest, TotalsAndStreakMatchTheServiceForAKnownHistory) {
            World world;
            // No goal, so every day with a run counts — which is what a reader
            // of "three days in a row" expects this to be measuring.
            world.config.goals = {.daily_minutes = 0, .daily_runs = 0};
            world.record("timed", 60.0, 2);
            world.record("timed", 70.0, 1);
            world.record("timed", 80.0, 0);
            HistoryScreen screen{world.context};

            EXPECT_EQ(screen.data().totals.sessions, 3U);
            EXPECT_DOUBLE_EQ(screen.data().totals.mean_net_wpm.value, 70.0);
            EXPECT_EQ(screen.data().streak.current, 3U) << "three days in a row";
            EXPECT_EQ(screen.data().streak.longest, 3U);
        }

        TEST(HistoryScreenTest, TheStreakOnScreenRespectsTheConfiguredGoal) {
            // GAMEPLAY §7.4: consecutive days *with the goal met*. Three
            // thirty-second runs are three days somebody turned up and no days
            // they practised, and the screen must not call that a streak of
            // three when the configuration says ten minutes.
            World world;
            world.config.goals = {.daily_minutes = 10, .daily_runs = 5};
            world.record("timed", 60.0, 2);
            world.record("timed", 70.0, 1);
            world.record("timed", 80.0, 0);
            HistoryScreen screen{world.context};

            EXPECT_EQ(screen.data().streak.current, 0U);
            EXPECT_EQ(screen.data().today.runs, 1U) << "and today is still counted";
            EXPECT_FALSE(screen.data().today.met);
        }

        TEST(HistoryScreenTest, TodaysProgressIsShownEitherWay) {
            // "Goal met" alone leaves somebody wondering whether the line
            // failed to draw or the day did.
            World world;
            world.config.goals = {.daily_minutes = 0, .daily_runs = 1};
            world.record("timed", 80.0, 0);
            HistoryScreen screen{world.context};

            EXPECT_TRUE(screen.data().today.met);
            EXPECT_NE(testing::render_to_text(screen.render(), 80, 30).find("goal met"), std::string::npos);
        }

        TEST(HistoryScreenTest, TheModeFilterChangesWhatIsShown) {
            World world;
            world.record("timed", 60.0, 1);
            world.record("quote", 90.0, 0);
            HistoryScreen screen{world.context};
            ASSERT_EQ(screen.data().sessions.size(), 2U);

            // Right from `all` lands on the first real mode.
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));

            ASSERT_TRUE(screen.filter().mode.has_value());
            for (const app::SessionRow& row: screen.data().sessions) {
                EXPECT_EQ(row.mode, *screen.filter().mode);
            }
        }

        TEST(HistoryScreenTest, TheRangeFilterChangesWhatIsShown) {
            World world;
            world.record("timed", 60.0, 100);  // Outside a 30-day window.
            world.record("timed", 80.0, 1);
            HistoryScreen screen{world.context};
            ASSERT_EQ(screen.data().sessions.size(), 2U);

            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));  // Mode → range.
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));  // all → 7 days.

            EXPECT_TRUE(screen.filter().since.has_value());
            EXPECT_EQ(screen.data().sessions.size(), 1U) << "the hundred-day-old run is out of range";
        }

        TEST(HistoryScreenTest, TheTwoFiltersCombine) {
            World world;
            world.record("timed", 60.0, 100);
            world.record("quote", 65.0, 100);
            world.record("timed", 80.0, 1);
            world.record("quote", 85.0, 1);
            HistoryScreen screen{world.context};
            ASSERT_EQ(screen.data().sessions.size(), 4U);

            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));  // A mode.
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));  // And a range.

            ASSERT_TRUE(screen.filter().mode.has_value());
            ASSERT_TRUE(screen.filter().since.has_value());
            EXPECT_EQ(screen.data().sessions.size(), 1U) << "one mode, inside the window";
        }

        TEST(HistoryScreenTest, AbandonedRunsAreShownBecauseTheyHappened) {
            World world;
            world.record("timed", 60.0, 1, /*completed=*/false);
            HistoryScreen screen{world.context};

            EXPECT_EQ(screen.data().sessions.size(), 1U);
            EXPECT_FALSE(screen.filter().completed_only);
            EXPECT_NE(testing::render_to_text(screen.render(), 80, 24).find("abandoned"), std::string::npos);
        }

        TEST(HistoryScreenTest, TheSessionListScrollsAndPagesWithoutRunningOffEitherEnd) {
            World world;
            for (std::int64_t at = 0; at < 40; ++at) {
                world.record("timed", 60.0, at);
            }
            HistoryScreen screen{world.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));  // Mode → range → sessions.
            ASSERT_EQ(screen.focused(), HistoryField::Sessions);

            EXPECT_EQ(screen.selected(), 0U);
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowUp));
            EXPECT_EQ(screen.selected(), 0U) << "clamped at the top rather than wrapping to the end";

            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowDown));
            EXPECT_EQ(screen.selected(), 1U);

            for (int page = 0; page < 20; ++page) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::PageDown));
            }
            EXPECT_EQ(screen.selected(), 39U) << "paging past the end lands on the last row";
        }

        TEST(HistoryScreenTest, SelectingARowReportsTheSessionToOpen) {
            // Reported, not pushed: the screen never touches the stack, which
            // is what keeps navigation in one place (TI-081).
            World world;
            world.record("timed", 60.0, 1);
            world.record("timed", 80.0, 0);
            HistoryScreen screen{world.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Tab));

            EXPECT_FALSE(screen.take_opened().has_value()) << "nothing asked for yet";
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            const std::optional<core::SessionId> opened = screen.take_opened();
            ASSERT_TRUE(opened.has_value());
            EXPECT_EQ(*opened, screen.data().sessions.front().id);
            EXPECT_FALSE(screen.take_opened().has_value()) << "and it is taken exactly once";
        }

        TEST(HistoryScreenTest, PersonalBestsAreListedPerModeAndParameter) {
            // A 15-second best and a 60-second best are separate records
            // because they measure different things (GAMEPLAY §7.3).
            World world;
            world.record("timed", 80.0, 1);
            world.record("quote", 90.0, 0);
            HistoryScreen screen{world.context};

            EXPECT_FALSE(screen.data().bests.empty());
            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);
            EXPECT_NE(drawn.find("bests"), std::string::npos) << drawn;
        }

        TEST(HistoryScreenTest, AFailingQueryIsReportedInPlaceRatherThanBlankingTheScreen) {
            World world;
            world.record("timed", 60.0, 1);
            world.history.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the fixture asked it to"));
            HistoryScreen screen{world.context};

            EXPECT_FALSE(screen.data().problems.empty()) << "the failure is recorded";
            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);
            EXPECT_FALSE(drawn.empty());
        }

        TEST(HistoryScreenTest, NothingIsQueriedByDrawing) {
            // The whole reason the data lives in a member. A query inside a
            // render callback runs sixty times a second against a file on disk.
            World world;
            world.record("timed", 60.0, 1);
            HistoryScreen screen{world.context};
            const std::size_t after_load = world.history.queries;

            static_cast<void>(testing::render_to_text(screen.render(), 80, 24));
            static_cast<void>(testing::render_to_text(screen.render(), 80, 24));

            EXPECT_EQ(world.history.queries, after_load) << "drawing asked the database nothing";
        }

        // --- export (TI-108) ----------------------------------------------------

        /// Opens the export prompt and types `path` into it.
        void type_export_path(HistoryScreen& screen, std::string_view path) {
            ASSERT_TRUE(screen.on_event(ftxui::Event::Special(std::string(1, '\x05'))));  // ctrl-e
            ASSERT_TRUE(screen.exporting());
            for (const char letter: path) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::Character(letter)));
            }
        }

        TEST(HistoryScreenTest, ExportWritesToTheChosenPathAndSaysSo) {
            World world;
            world.record("timed", 60.0, 1);
            HistoryScreen screen{world.context};

            type_export_path(screen, "/tmp/typeit-history.csv");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_EQ(world.written_to, std::filesystem::path{"/tmp/typeit-history.csv"});
            EXPECT_FALSE(world.written.empty());
            EXPECT_NE(screen.export_message().find("wrote"), std::string::npos) << screen.export_message();
            EXPECT_FALSE(screen.exporting()) << "and the prompt closes";
        }

        TEST(HistoryScreenTest, AnUnwritablePathIsReportedRatherThanSilentlyFailing) {
            World world;
            world.record("timed", 60.0, 1);
            world.refuse = true;
            HistoryScreen screen{world.context};

            type_export_path(screen, "/nowhere/at/all.csv");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_NE(screen.export_message().find("cannot write"), std::string::npos) << screen.export_message();
            EXPECT_NE(testing::render_to_text(screen.render(), 80, 30).find("cannot write"), std::string::npos);
        }

        TEST(HistoryScreenTest, ExportedContentMatchesTheCliExportByteForByte) {
            // The same service, the same filter, the same bytes. A second
            // formatter here would drift from the CLI's within a release.
            World world;
            world.record("timed", 60.0, 1);
            world.record("quote", 90.0, 0);
            HistoryScreen screen{world.context};

            type_export_path(screen, "history.csv");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));
            const core::Result<std::string> from_cli = world.service.to_csv(screen.filter());

            ASSERT_TRUE(from_cli);
            EXPECT_EQ(world.written, *from_cli);
        }

        TEST(HistoryScreenTest, TheExtensionChoosesTheFormat) {
            World world;
            world.record("timed", 60.0, 1);
            HistoryScreen screen{world.context};

            type_export_path(screen, "history.json");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));
            const core::Result<std::string> from_cli = world.service.to_json(screen.filter());

            ASSERT_TRUE(from_cli);
            EXPECT_EQ(world.written, *from_cli);
            EXPECT_TRUE(world.written.starts_with("[")) << world.written.substr(0, 40);
        }

        TEST(HistoryScreenTest, TheExportFilterIsTheOneOnScreen) {
            // What lands in the file is what the user was looking at, not the
            // whole history — a filtered view that exported everything would
            // be a quiet surprise in somebody's spreadsheet.
            World world;
            world.record("timed", 60.0, 1);
            world.record("quote", 90.0, 0);
            HistoryScreen screen{world.context};
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowRight));  // Pick a mode.
            ASSERT_EQ(screen.data().sessions.size(), 1U);

            type_export_path(screen, "history.csv");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            // Header plus one row, and no more.
            EXPECT_EQ(std::ranges::count(world.written, '\n'), 2) << world.written;
        }

        TEST(HistoryScreenTest, AnEmptyPathIsRefusedWithAMessage) {
            World world;
            world.record("timed", 60.0, 1);
            HistoryScreen screen{world.context};

            type_export_path(screen, "");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_TRUE(world.written.empty());
            EXPECT_FALSE(screen.export_message().empty());
            EXPECT_TRUE(screen.exporting()) << "and the prompt stays open to be corrected";
        }

        TEST(HistoryScreenTest, EscapeCancelsTheExportPrompt) {
            World world;
            HistoryScreen screen{world.context};

            type_export_path(screen, "history.csv");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Escape));

            EXPECT_FALSE(screen.exporting());
            EXPECT_TRUE(world.written.empty());
        }

        TEST(HistoryScreenTest, ThePromptOwnsTheKeyboardWhileItIsOpen) {
            // A path containing a letter must not cycle a filter behind it.
            World world;
            world.record("timed", 60.0, 1);
            HistoryScreen screen{world.context};
            const std::optional<std::string> before = screen.filter().mode;

            type_export_path(screen, "a-path");

            EXPECT_EQ(screen.filter().mode, before);
            EXPECT_EQ(screen.export_path(), "a-path");
        }

        TEST(HistoryScreenTest, WithNoWriterExportingSaysSoRatherThanDoingNothing) {
            World world;
            world.context.save_text = {};
            HistoryScreen screen{world.context};

            type_export_path(screen, "history.csv");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_NE(screen.export_message().find("not available"), std::string::npos) << screen.export_message();
        }

        TEST(HistoryScreenTest, SnapshotAt80x24) {
            World world;
            world.theme.name = "snapshot";
            for (std::int64_t at = 0; at < 5; ++at) {
                world.record("timed", 60.0 + static_cast<double>(at) * 4.0, 4 - at);
            }
            HistoryScreen screen{world.context};

            testing::expect_matches_golden("history_80x24", testing::render_to_text(screen.render(), 80, 24));
        }

        TEST(HistoryScreenTest, SnapshotAt120x40) {
            World world;
            world.theme.name = "snapshot";
            world.context.size = TerminalSize{.columns = 120, .rows = 40};
            for (std::int64_t at = 0; at < 5; ++at) {
                world.record("timed", 60.0 + static_cast<double>(at) * 4.0, 4 - at);
            }
            HistoryScreen screen{world.context};

            testing::expect_matches_golden("history_120x40", testing::render_to_text(screen.render(), 120, 40));
        }

    }  // namespace
}  // namespace typeit::tui
