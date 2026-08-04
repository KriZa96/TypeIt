// What the contract suites cannot say about a fake (TI-064).
//
// The suites prove the fakes behave like the real adapters. These prove the two
// things only a fake has: it can be made to fail on demand, and it remembers
// what it was asked to do. Both exist for Phase 3's service tests — an error
// path nobody can produce is an error path nobody has tested, and an
// orchestration nobody can observe is one nobody can assert on.

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "typeit/app/records/History.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/Fakes.h"

namespace typeit::testing {
    namespace {

        constexpr std::int64_t kMillisPerDay = 86'400'000;

        app::SessionRecord a_run() {
            app::SessionRecord record;
            record.mode = "timed";
            record.completed = true;
            record.accuracy = core::Accuracy{0.99};
            record.net_wpm = core::Wpm{100.0};
            return record;
        }

        TEST(FakesTest, AnArmedFailureIsWhatHappensInstead) {
            FakeHistoryRepository repository;
            repository.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "the disk is on fire"));

            const core::Result<core::SessionId> saved = repository.save(a_run());

            ASSERT_FALSE(saved);
            EXPECT_EQ(saved.error().code, core::ErrorCode::DbQuery);
            EXPECT_EQ(saved.error().context, "the disk is on fire");
            EXPECT_EQ(repository.saves, 0U) << "and the call did not happen";
        }

        TEST(FakesTest, AFailureIsArmedOnceAndThenGone) {
            // "The next save fails" is a scenario. "Everything fails from now
            // on" is a different object.
            FakeHistoryRepository repository;
            repository.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "once"));

            EXPECT_FALSE(repository.save(a_run()));
            EXPECT_TRUE(repository.save(a_run())) << "the second call is a normal one";
            EXPECT_EQ(repository.saves, 1U);
        }

        TEST(FakesTest, EveryFakeCanBeMadeToFail) {
            FakeHistoryRepository history;
            FakeTextLibraryRepository library;
            FakeConfigStore config;
            FakeFileSystem files;
            FakeAssetLocator assets;

            history.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "no"));
            library.failure.fail_next(core::make_error(core::ErrorCode::DbQuery, "no"));
            config.failure.fail_next(core::make_error(core::ErrorCode::ConfigParse, "no"));
            files.failure.fail_next(core::make_error(core::ErrorCode::FileUnreadable, "no"));
            assets.failure.fail_next(core::make_error(core::ErrorCode::FileNotFound, "no"));

            EXPECT_FALSE(history.query({}));
            EXPECT_FALSE(library.list({}));
            EXPECT_FALSE(config.load());
            EXPECT_FALSE(files.read_text("/anything"));
            EXPECT_FALSE(assets.locate("texts"));
        }

        TEST(FakesTest, TheCallCountsAreAccurate) {
            // What a service test asserts on: that the orchestration did the
            // steps, in the number expected.
            FakeHistoryRepository repository;

            ASSERT_TRUE(repository.save(a_run()));
            ASSERT_TRUE(repository.save(a_run()));
            ASSERT_TRUE(repository.merge_key_stats({}));

            EXPECT_EQ(repository.saves, 2U);
            EXPECT_EQ(repository.merges, 1U);
            EXPECT_EQ(repository.records.size(), 2U) << "and what it was called with";
        }

        TEST(FakesTest, TheConfigStoreRemembersWhatWasSaved) {
            FakeConfigStore store;
            core::Config config;
            config.general.default_duration_s = 45;

            ASSERT_TRUE(store.save(config));

            EXPECT_EQ(store.saves, 1U);
            EXPECT_EQ(store.loaded.config.general.default_duration_s, 45);
        }

        TEST(FakesTest, TheFileSystemBuildsItsParentDirectories) {
            // So a test writes one line rather than a tree.
            FakeFileSystem files;

            files.add_file("/library/texts/prose.txt", "the quick brown fox");

            EXPECT_TRUE(files.exists("/library/texts/prose.txt"));
            EXPECT_TRUE(files.is_directory("/library/texts"));
            EXPECT_TRUE(files.is_directory("/library"));
        }

        TEST(FakesTest, TheAssetLocatorAnswersOnlyForWhatItWasGiven) {
            FakeAssetLocator assets;
            assets.kinds["texts"] = "/usr/share/typeit/texts";

            const core::Result<std::filesystem::path> texts = assets.locate("texts");
            const core::Result<std::filesystem::path> themes = assets.locate("themes");

            ASSERT_TRUE(texts);
            EXPECT_EQ(*texts, "/usr/share/typeit/texts");
            EXPECT_FALSE(themes) << "and says so rather than inventing a path";
        }

        TEST(FakesTest, TheHistoryWindowIsMeasuredAgainstAToldClock) {
            // A fake that asked the real clock would make every window test
            // depend on the day it runs.
            FakeHistoryRepository repository;
            repository.now = core::Millis{1'000 * kMillisPerDay};

            app::SessionRecord inside = a_run();
            inside.started_at = core::Millis{repository.now.value - (5 * kMillisPerDay)};
            inside.peak_wpm = core::Wpm{80.0};
            app::SessionRecord outside = a_run();
            outside.started_at = core::Millis{repository.now.value - (100 * kMillisPerDay)};
            outside.peak_wpm = core::Wpm{200.0};
            ASSERT_TRUE(repository.save(inside));
            ASSERT_TRUE(repository.save(outside));

            const core::Result<core::Wpm> best = repository.best_sustained_wpm(core::Days{30});

            ASSERT_TRUE(best);
            EXPECT_DOUBLE_EQ(best->value, 80.0);
        }

    }  // namespace
}  // namespace typeit::testing
