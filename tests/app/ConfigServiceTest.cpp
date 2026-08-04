#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "typeit/app/services/ConfigService.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        using core::Config;
        using core::ErrorCode;
        using core::Status;

        class ConfigServiceTest : public ::testing::Test {
        protected:
            testing::FakeConfigStore store_;
            ConfigService service_{store_};
        };

        // ---- loading -------------------------------------------------------

        TEST_F(ConfigServiceTest, LoadingKeepsWhatTheStoreGave) {
            store_.loaded.config.general.default_duration_s = 45;
            store_.loaded.warnings = {"appearance.wat: unknown setting; ignored"};

            const Status loaded = service_.load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(service_.config().general.default_duration_s, 45);
            EXPECT_EQ(service_.warnings().size(), 1U) << "and what it complained about";
        }

        TEST_F(ConfigServiceTest, ALoadFailureFallsBackToDefaultsAndStillReportsIt) {
            // Both halves. The application must start — a broken config is not a
            // reason to be unusable — and the person whose file could not be
            // read must be told, or they spend an evening wondering why their
            // settings do nothing.
            store_.loaded.config.general.default_duration_s = 45;
            store_.failure.fail_next(core::make_error(ErrorCode::ConfigParse, "config.toml:3:1: expected a key"));

            const Status loaded = service_.load();

            ASSERT_FALSE(loaded) << "the error reaches the caller";
            EXPECT_EQ(loaded.error().code, ErrorCode::ConfigParse);
            EXPECT_NE(loaded.error().context.find("config.toml:3:1"), std::string::npos);
            EXPECT_EQ(service_.config().general.default_duration_s, Config{}.general.default_duration_s)
                    << "and the application has usable settings anyway";
        }

        TEST_F(ConfigServiceTest, AConfigThatIsSyntacticallyFineButImpossibleIsReset) {
            // A hand-edited file, or one written by a newer version and then
            // downgraded into. The store reverts what it can attribute; what is
            // left is caught here.
            store_.loaded.config.race.lead_danger = 90;
            store_.loaded.config.race.lead_comfort = 10;

            const Status loaded = service_.load();

            ASSERT_TRUE(loaded) << "still not a reason to refuse to start";
            EXPECT_EQ(service_.config().race.lead_danger, Config{}.race.lead_danger);
            ASSERT_FALSE(service_.warnings().empty());
            EXPECT_NE(service_.warnings().back().find("lead_danger"), std::string::npos) << service_.warnings().back();
        }

        TEST_F(ConfigServiceTest, LoadingTwiceDoesNotAccumulateWarnings) {
            store_.loaded.warnings = {"appearance.wat: unknown setting; ignored"};

            ASSERT_TRUE(service_.load());
            ASSERT_TRUE(service_.load());

            EXPECT_EQ(service_.warnings().size(), 1U);
        }

        TEST_F(ConfigServiceTest, BeforeAnyLoadTheConfigIsTheDefaults) {
            EXPECT_EQ(service_.config().general.default_mode, Config{}.general.default_mode);
            EXPECT_TRUE(service_.warnings().empty());
        }

        // ---- saving --------------------------------------------------------

        TEST_F(ConfigServiceTest, SavingWritesAndAdopts) {
            Config config;
            config.general.default_duration_s = 60;

            const Status saved = service_.save(config);

            ASSERT_TRUE(saved) << (saved ? "" : saved.error().context);
            EXPECT_EQ(store_.saves, 1U);
            EXPECT_EQ(service_.config().general.default_duration_s, 60);
        }

        TEST_F(ConfigServiceTest, AnInvalidConfigIsRefusedRatherThanWritten) {
            // The file on disk should never be something this program would
            // itself refuse to load.
            Config invalid;
            invalid.general.default_duration_s = 0;

            const Status saved = service_.save(invalid);

            ASSERT_FALSE(saved);
            EXPECT_EQ(saved.error().code, ErrorCode::ConfigInvalid);
            EXPECT_NE(saved.error().context.find("default_duration_s"), std::string::npos);
            EXPECT_EQ(store_.saves, 0U) << "nothing was written";
            EXPECT_EQ(service_.config().general.default_duration_s, Config{}.general.default_duration_s)
                    << "and nothing was adopted";
        }

        TEST_F(ConfigServiceTest, AFailedWriteLeavesTheInMemoryConfigAlone) {
            // A settings screen that says "saved" over a full disk has lied.
            Config config;
            config.general.default_duration_s = 60;
            store_.failure.fail_next(core::make_error(ErrorCode::FileUnreadable, "config.toml: no space left"));

            const Status saved = service_.save(config);

            ASSERT_FALSE(saved);
            EXPECT_EQ(service_.config().general.default_duration_s, Config{}.general.default_duration_s);
        }

        // ---- notification --------------------------------------------------

        TEST_F(ConfigServiceTest, EverySuccessfulSaveNotifiesOnceWithTheNewValue) {
            std::vector<std::int64_t> seen;
            service_.on_change([&seen](const Config& config) { seen.push_back(config.general.default_duration_s); });

            Config config;
            config.general.default_duration_s = 60;
            ASSERT_TRUE(service_.save(config));
            config.general.default_duration_s = 15;
            ASSERT_TRUE(service_.save(config));

            EXPECT_EQ(seen, (std::vector<std::int64_t>{60, 15}));
        }

        TEST_F(ConfigServiceTest, EveryObserverIsCalled) {
            int first = 0;
            int second = 0;
            service_.on_change([&first](const Config&) { ++first; });
            service_.on_change([&second](const Config&) { ++second; });

            ASSERT_TRUE(service_.save(Config{}));

            EXPECT_EQ(first, 1);
            EXPECT_EQ(second, 1);
            EXPECT_EQ(service_.observer_count(), 2U);
        }

        TEST_F(ConfigServiceTest, ARefusedOrFailedSaveNotifiesNobody) {
            int calls = 0;
            service_.on_change([&calls](const Config&) { ++calls; });

            Config invalid;
            invalid.general.countdown_s = 99;
            ASSERT_FALSE(service_.save(invalid));

            store_.failure.fail_next(core::make_error(ErrorCode::FileUnreadable, "no space"));
            ASSERT_FALSE(service_.save(Config{}));

            EXPECT_EQ(calls, 0) << "nothing changed, so nothing is announced";
        }

        TEST_F(ConfigServiceTest, LoadingNotifiesNobody) {
            // At load nothing has changed yet, and an observer that fires
            // during construction is an observer nobody has registered.
            int calls = 0;
            service_.on_change([&calls](const Config&) { ++calls; });

            ASSERT_TRUE(service_.load());

            EXPECT_EQ(calls, 0);
        }

    }  // namespace
}  // namespace typeit::app
