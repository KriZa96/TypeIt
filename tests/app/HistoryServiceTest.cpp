// Trends, streaks and export (TI-069).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        constexpr std::int64_t kMillisPerDay = 86'400'000;
        constexpr std::int64_t kMillisPerHour = 3'600'000;
        /// A Thursday, so the Monday-start week logic has something to do.
        constexpr core::Millis kDayZero{1'000 * kMillisPerDay};

        class HistoryServiceTest : public ::testing::Test {
        protected:
            /// A run `days_ago` days back, at `hour` local-UTC o'clock.
            void a_run(std::int64_t days_ago, double net_wpm, std::int64_t hour = 12) {
                SessionRecord record;
                record.mode = "timed";
                record.mode_param = R"({"seconds":30})";
                record.completed = true;
                record.started_at = core::Millis{kDayZero.value - (days_ago * kMillisPerDay) + (hour * kMillisPerHour)};
                record.duration = core::Millis{30'000};
                record.net_wpm = core::Wpm{net_wpm};
                record.gross_wpm = core::Wpm{net_wpm + 2.0};
                record.accuracy = core::Accuracy{0.98};
                record.consistency = 80.0;
                ASSERT_TRUE(history_.save(record));
            }

            [[nodiscard]] std::vector<TrendPoint> trend(TrendBucket bucket, UtcOffsetMinutes offset = 0) {
                const core::Result<std::vector<TrendPoint>> points = service_.trend({}, bucket, offset);
                EXPECT_TRUE(points) << (points ? "" : points.error().context);
                return points.value_or(std::vector<TrendPoint>{});
            }

            testing::FakeHistoryRepository history_;
            HistoryService service_{history_};
        };

        // ---- trends ---------------------------------------------------------

        TEST_F(HistoryServiceTest, AnEmptyHistoryHasAnEmptyTrend) {
            EXPECT_TRUE(trend(TrendBucket::Day).empty()) << "empty results, never NaN and never a crash";
        }

        TEST_F(HistoryServiceTest, RunsOnOneDayAreOnePoint) {
            a_run(0, 60.0, 9);
            a_run(0, 80.0, 17);

            const std::vector<TrendPoint> points = trend(TrendBucket::Day);

            ASSERT_EQ(points.size(), 1U);
            EXPECT_EQ(points.front().sessions, 2U);
            EXPECT_DOUBLE_EQ(points.front().mean_net_wpm.value, 70.0) << "the mean, not the last";
            EXPECT_EQ(points.front().total_time, core::Millis{60'000});
        }

        TEST_F(HistoryServiceTest, TheTrendReadsOldestFirst) {
            a_run(0, 90.0);
            a_run(2, 70.0);
            a_run(1, 80.0);

            const std::vector<TrendPoint> points = trend(TrendBucket::Day);

            ASSERT_EQ(points.size(), 3U);
            EXPECT_DOUBLE_EQ(points[0].mean_net_wpm.value, 70.0);
            EXPECT_DOUBLE_EQ(points[1].mean_net_wpm.value, 80.0);
            EXPECT_DOUBLE_EQ(points[2].mean_net_wpm.value, 90.0) << "a trend line is drawn left to right";
            EXPECT_LT(points[0].start, points[1].start);
        }

        TEST_F(HistoryServiceTest, ADayWithNoRunsIsNotAPoint) {
            a_run(0, 90.0);
            a_run(3, 70.0);

            EXPECT_EQ(trend(TrendBucket::Day).size(), 2U)
                    << "a gap drawn as zero WPM says somebody typed badly on a day they did not type";
        }

        TEST_F(HistoryServiceTest, WeeksBucketSevenDaysTogether) {
            for (std::int64_t day = 0; day < 7; ++day) {
                a_run(day, 70.0);
            }

            const std::vector<TrendPoint> days = trend(TrendBucket::Day);
            const std::vector<TrendPoint> weeks = trend(TrendBucket::Week);

            EXPECT_EQ(days.size(), 7U);
            EXPECT_LE(weeks.size(), 2U) << "seven consecutive days span at most two Monday-start weeks";
            std::size_t total = 0;
            for (const TrendPoint& point: weeks) {
                total += point.sessions;
            }
            EXPECT_EQ(total, 7U) << "and nothing is lost in the bucketing";
        }

        TEST_F(HistoryServiceTest, WeeksBeginOnMonday) {
            // 1 January 1970 was a Thursday, which is the only fact this needs.
            constexpr core::Millis kThursday{0};
            constexpr core::Millis kMondayBefore{-3 * kMillisPerDay};

            SessionRecord record;
            record.mode = "timed";
            record.completed = true;
            record.started_at = kThursday;
            ASSERT_TRUE(history_.save(record));

            const core::Result<std::vector<TrendPoint>> points = service_.trend({}, TrendBucket::Week);
            ASSERT_TRUE(points);
            ASSERT_EQ(points->size(), 1U);
            EXPECT_EQ(points->front().start, kMondayBefore);
        }

        TEST_F(HistoryServiceTest, DaysAreLocalDays) {
            // 23:30 UTC is the next day in Berlin. Two runs half an hour apart
            // are one day or two depending on where the typist is, and the
            // typist's answer is the right one.
            a_run(0, 60.0, 23);
            a_run(0, 80.0, 22);

            EXPECT_EQ(trend(TrendBucket::Day, 0).size(), 1U) << "one day in UTC";
            EXPECT_EQ(trend(TrendBucket::Day, 60).size(), 2U) << "and two an hour east of it";
        }

        // ---- streaks --------------------------------------------------------

        TEST_F(HistoryServiceTest, NoRunsIsNoStreak) {
            const core::Result<Streak> streak = service_.streak({}, kDayZero);

            ASSERT_TRUE(streak);
            EXPECT_EQ(streak->current, 0U);
            EXPECT_EQ(streak->longest, 0U);
        }

        TEST_F(HistoryServiceTest, ConsecutiveDaysAreAStreak) {
            a_run(0, 60.0);
            a_run(1, 60.0);
            a_run(2, 60.0);

            const core::Result<Streak> streak = service_.streak({}, kDayZero);

            ASSERT_TRUE(streak);
            EXPECT_EQ(streak->current, 3U);
            EXPECT_EQ(streak->longest, 3U);
        }

        TEST_F(HistoryServiceTest, TwoRunsInOneDayAreOneDay) {
            a_run(0, 60.0, 9);
            a_run(0, 60.0, 21);
            a_run(1, 60.0);

            const core::Result<Streak> streak = service_.streak({}, kDayZero);

            ASSERT_TRUE(streak);
            EXPECT_EQ(streak->current, 2U) << "enthusiasm is not a second day";
        }

        TEST_F(HistoryServiceTest, ABrokenStreakStopsBeingCurrentButIsStillTheLongest) {
            a_run(10, 60.0);
            a_run(11, 60.0);
            a_run(12, 60.0);
            a_run(13, 60.0);

            const core::Result<Streak> streak = service_.streak({}, kDayZero);

            ASSERT_TRUE(streak);
            EXPECT_EQ(streak->current, 0U) << "telling somebody they are on day four when they stopped last week";
            EXPECT_EQ(streak->longest, 4U);
        }

        TEST_F(HistoryServiceTest, YesterdayStillCounts) {
            // Somebody who has not typed *yet today* has not lost their streak.
            a_run(1, 60.0);
            a_run(2, 60.0);

            const core::Result<Streak> streak = service_.streak({}, kDayZero);

            ASSERT_TRUE(streak);
            EXPECT_EQ(streak->current, 2U);
        }

        TEST_F(HistoryServiceTest, StreaksAreCountedInLocalDays) {
            a_run(0, 60.0, 23);
            a_run(1, 60.0, 23);

            const core::Result<Streak> utc = service_.streak({}, kDayZero, 0);
            const core::Result<Streak> east = service_.streak({}, kDayZero, 60);

            ASSERT_TRUE(utc);
            ASSERT_TRUE(east);
            EXPECT_EQ(utc->current, 2U);
            EXPECT_EQ(east->longest, 2U) << "the same two days, one time zone over";
        }

        // ---- filters --------------------------------------------------------

        TEST_F(HistoryServiceTest, FiltersReachTheRepository) {
            a_run(0, 60.0);
            a_run(1, 60.0);

            HistoryFilter filter;
            filter.limit = 1;
            const core::Result<std::vector<TrendPoint>> points = service_.trend(filter, TrendBucket::Day);

            ASSERT_TRUE(points);
            EXPECT_EQ(points->size(), 1U) << "the service asks the question it was given";
        }

        TEST_F(HistoryServiceTest, AFailureToQueryIsReported) {
            history_.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the database is locked"));

            const core::Result<std::vector<TrendPoint>> points = service_.trend({}, TrendBucket::Day);

            ASSERT_FALSE(points);
            EXPECT_EQ(points.error().code, core::ErrorCode::DbQuery);
        }

        // ---- CSV ------------------------------------------------------------

        TEST_F(HistoryServiceTest, TheCsvHasItsHeaderAndOneLinePerRun) {
            a_run(0, 60.0);
            a_run(1, 70.0);

            const core::Result<std::string> csv = service_.to_csv({});

            ASSERT_TRUE(csv) << (csv ? "" : csv.error().context);
            EXPECT_TRUE(csv->starts_with(
                    "id,started_at,mode,mode_param,duration_ms,net_wpm,gross_wpm,accuracy,consistency,completed\n"))
                    << *csv;
            EXPECT_EQ(std::ranges::count(*csv, '\n'), 3) << "header plus two runs";
        }

        TEST_F(HistoryServiceTest, AnEmptyHistoryIsAHeaderAndNothingElse) {
            const core::Result<std::string> csv = service_.to_csv({});

            ASSERT_TRUE(csv);
            EXPECT_EQ(std::ranges::count(*csv, '\n'), 1);
        }

        TEST_F(HistoryServiceTest, CommasQuotesAndNewlinesAreEscaped) {
            SessionRecord record;
            record.mode = "quote";
            // `mode_param` is JSON, so it has commas and quotes in it always;
            // the newline is what an imported title can carry in.
            record.mode_param = R"({"title":"Say \"hi\", then, stop","note":"two)"
                                "\n"
                                R"(lines"})";
            record.completed = true;
            ASSERT_TRUE(history_.save(record));

            const core::Result<std::string> csv = service_.to_csv({});

            ASSERT_TRUE(csv);
            EXPECT_NE(csv->find(R"("{""title"":""Say \""hi\"", then, stop"")"), std::string::npos) << *csv;
            EXPECT_EQ(std::ranges::count(*csv, '\n'), 3) << "the embedded newline is inside a quoted field";
        }

        TEST_F(HistoryServiceTest, AFieldWithNothingSpecialIsNotQuoted) {
            a_run(0, 60.0);

            const core::Result<std::string> csv = service_.to_csv({});

            ASSERT_TRUE(csv);
            EXPECT_NE(csv->find(",timed,"), std::string::npos) << "quoting everything would be noise: " << *csv;
        }

        // ---- JSON -----------------------------------------------------------

        TEST_F(HistoryServiceTest, AnEmptyHistoryIsAnEmptyArray) {
            const core::Result<std::string> json = service_.to_json({});

            ASSERT_TRUE(json);
            EXPECT_EQ(*json, "[]") << "not null, and not nothing at all";
        }

        TEST_F(HistoryServiceTest, TheJsonCarriesTheNumbersAndTheFlags) {
            a_run(0, 60.0);

            const core::Result<std::string> json = service_.to_json({});

            ASSERT_TRUE(json) << (json ? "" : json.error().context);
            EXPECT_TRUE(json->starts_with("[{")) << *json;
            EXPECT_TRUE(json->ends_with("}]")) << *json;
            EXPECT_NE(json->find(R"("mode":"timed")"), std::string::npos) << *json;
            EXPECT_NE(json->find(R"("completed":true)"), std::string::npos) << "a flag, not the string \"1\"";
        }

        TEST_F(HistoryServiceTest, QuotesAndControlCharactersAreEscaped) {
            SessionRecord record;
            record.mode = "quote";
            record.mode_param = "a \"quoted\" \\ backslash\nand a newline";
            record.completed = true;
            ASSERT_TRUE(history_.save(record));

            const core::Result<std::string> json = service_.to_json({});

            ASSERT_TRUE(json);
            EXPECT_NE(json->find(R"(a \"quoted\" \\ backslash\nand a newline)"), std::string::npos) << *json;
            EXPECT_EQ(std::ranges::count(*json, '\n'), 0) << "a raw newline inside a string is not JSON";
        }

        TEST_F(HistoryServiceTest, NonAsciiIsPreserved) {
            SessionRecord record;
            record.mode = "quote";
            record.mode_param = R"({"title":"čšž 漢字"})";
            record.completed = true;
            ASSERT_TRUE(history_.save(record));

            const core::Result<std::string> json = service_.to_json({});

            ASSERT_TRUE(json);
            EXPECT_NE(json->find("čšž 漢字"), std::string::npos) << "the file is UTF-8: " << *json;
        }

    }  // namespace
}  // namespace typeit::app
