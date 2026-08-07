// One past run, in full (TI-104).
//
// The assertions worth having are about the rows a naive implementation
// crashes or lies on: a run whose text has since been deleted, a run recorded
// by a version whose metrics meant something else, and an id nobody recorded.

#include <cstddef>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Snapshot.h"
#include "screens/SessionDetailScreen.h"
#include "typeit/app/records/History.h"
#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Fakes.h"

namespace typeit::tui {
    namespace {

        constexpr core::Millis kNoon{1'767'225'600'000};

        struct World {
            app::Theme theme;
            Keymap keymap;
            core::Config config;
            testing::FakeHistoryRepository history;
            ScreenContext context;

            World() {
                context.theme = &theme;
                context.keymap = &keymap;
                context.config = &config;
                context.size = TerminalSize{.columns = 80, .rows = 30};
                context.history = HistorySource{.records = &history};
            }

            /// One run, saved, with its id handed back.
            core::SessionId save(const app::SessionRecord& record) {
                const core::Result<core::SessionId> id = history.save_run(record, {}, {});
                EXPECT_TRUE(id);
                return id ? *id : core::SessionId{0};
            }
        };

        app::SessionRecord a_run() {
            app::SessionRecord record;
            record.started_at = kNoon;
            record.ended_at = kNoon + core::Millis{30'000};
            record.mode = "timed";
            record.mode_param = R"({"seconds":30})";
            record.provider = "whole";
            record.duration = core::Millis{30'000};
            record.graphemes_typed = 300;
            record.graphemes_correct = 295;
            record.errors_total = 5;
            record.errors_uncorrected = 2;
            record.backspaces = 3;
            record.net_wpm = core::Wpm{72.5};
            record.gross_wpm = core::Wpm{80.0};
            record.raw_wpm = core::Wpm{82.0};
            record.accuracy = core::Accuracy{0.98};
            record.final_correctness = core::Accuracy{0.99};
            record.consistency = 88.0;
            record.completed = true;
            record.app_version = "2.0.0-alpha.6";
            for (std::int64_t second = 0; second < 10; ++second) {
                record.timeline.push_back({.at = core::Millis{second * 1'000},
                                           .wpm = core::Wpm{60.0 + static_cast<double>(second)},
                                           .keystrokes = 6,
                                           .errors = second == 3 ? 1U : 0U});
            }
            return record;
        }

        TEST(SessionDetailScreenTest, EveryStoredMetricRenders) {
            World world;
            const core::SessionId id = world.save(a_run());
            SessionDetailScreen screen{world.context, id};

            ASSERT_TRUE(screen.record().has_value()) << screen.problem();
            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);

            EXPECT_NE(drawn.find("72.5"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("98%"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("300"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("backspaces"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("uncorrected"), std::string::npos) << drawn;
        }

        TEST(SessionDetailScreenTest, TheTimelineIsDrawnAsAChartWithItsErrorsMarked) {
            World world;
            const core::SessionId id = world.save(a_run());
            SessionDetailScreen screen{world.context, id};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);

            EXPECT_NE(drawn.find('*'), std::string::npos) << "the per-second series:\n" << drawn;
            EXPECT_NE(drawn.find('x'), std::string::npos) << "and the second an error happened in:\n" << drawn;
        }

        TEST(SessionDetailScreenTest, ARunWithNoSamplesSaysSoRatherThanDrawingAnEmptyBox) {
            World world;
            app::SessionRecord bare = a_run();
            bare.timeline.clear();
            const core::SessionId id = world.save(bare);
            SessionDetailScreen screen{world.context, id};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);

            EXPECT_NE(drawn.find("no per-second samples"), std::string::npos) << drawn;
        }

        TEST(SessionDetailScreenTest, ARunWhoseTextHasSinceBeenDeletedRendersWithoutACrash) {
            // `text_id` is absent for generated text and for a library entry
            // somebody has removed. Neither is an error, and neither may be
            // read as text zero.
            World world;
            app::SessionRecord orphan = a_run();
            orphan.text_id.reset();
            const core::SessionId id = world.save(orphan);
            SessionDetailScreen screen{world.context, id};

            ASSERT_TRUE(screen.record().has_value());
            EXPECT_FALSE(screen.record()->text_id.has_value());
            EXPECT_FALSE(testing::render_to_text(screen.render(), 80, 30).empty());
        }

        TEST(SessionDetailScreenTest, ARunFromAnOlderMajorVersionIsShownWithANote) {
            // 2.0 changed what WPM and accuracy *mean* (VERSIONING §9), so a
            // 1.x run's numbers are not comparable with today's. Showing them
            // without saying so is the quiet kind of wrong.
            World world;
            app::SessionRecord old = a_run();
            old.app_version = "1.0.0";
            const core::SessionId id = world.save(old);
            SessionDetailScreen screen{world.context, id};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);

            EXPECT_NE(drawn.find("1.0.0"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("may not be comparable"), std::string::npos) << drawn;
        }

        TEST(SessionDetailScreenTest, ARunFromThisMajorVersionGetsNoNote) {
            World world;
            const core::SessionId id = world.save(a_run());
            SessionDetailScreen screen{world.context, id};

            EXPECT_EQ(testing::render_to_text(screen.render(), 80, 30).find("may not be comparable"),
                      std::string::npos);
        }

        TEST(SessionDetailScreenTest, AVersionNobodyWroteCountsAsNotThisOne) {
            // A row with no version, or one this cannot parse, is exactly the
            // row worth flagging — "unknown" is not "current".
            EXPECT_FALSE(recorded_by_this_major(""));
            EXPECT_FALSE(recorded_by_this_major("who knows"));
            EXPECT_FALSE(recorded_by_this_major("1.9.9"));
            EXPECT_TRUE(recorded_by_this_major("2.0.0-alpha.6"));
        }

        TEST(SessionDetailScreenTest, AnIdNobodyRecordedSaysSoRatherThanDrawingZeros) {
            World world;
            SessionDetailScreen screen{world.context, core::SessionId{9'999}};

            ASSERT_FALSE(screen.record().has_value());
            EXPECT_FALSE(screen.problem().empty());
            const std::string drawn = testing::render_to_text(screen.render(), 80, 30);
            EXPECT_EQ(drawn.find("0.0"), std::string::npos) << "no page of zeros:\n" << drawn;
        }

        TEST(SessionDetailScreenTest, WithNoHistorySourceItSaysSo) {
            World world;
            world.context.history = {};
            SessionDetailScreen screen{world.context, core::SessionId{1}};

            EXPECT_FALSE(screen.record().has_value());
            EXPECT_NE(testing::render_to_text(screen.render(), 80, 30).find("no history"), std::string::npos);
        }

        TEST(SessionDetailScreenTest, BackIsReportedOnceRatherThanPushedOrPopped) {
            World world;
            const core::SessionId id = world.save(a_run());
            SessionDetailScreen screen{world.context, id};

            EXPECT_FALSE(screen.take_back());
            ASSERT_TRUE(screen.on_event(ftxui::Event::Escape));
            EXPECT_TRUE(screen.take_back());
            EXPECT_FALSE(screen.take_back()) << "and taken exactly once";
        }

        TEST(SessionDetailScreenTest, NothingChangesByDrawing) {
            World world;
            const core::SessionId id = world.save(a_run());
            SessionDetailScreen screen{world.context, id};

            testing::expect_render_is_pure([&screen] { return screen.render(); }, 80, 30);
        }

        TEST(SessionDetailScreenTest, SnapshotAt80x30) {
            World world;
            world.theme.name = "snapshot";
            const core::SessionId id = world.save(a_run());
            SessionDetailScreen screen{world.context, id};

            testing::expect_matches_golden("session_detail_80x30",
                                           testing::render_to_text(screen.render(), 80, 30));
        }

    }  // namespace
}  // namespace typeit::tui
