#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/config/ConfigMigration.h"
#include "typeit/infra/config/TomlConfigStore.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        using core::Result;

        /// A rename table that does not exist in the shipped binary. The real
        /// one is empty — nothing has been renamed yet — and a migration nobody
        /// has run is a migration nobody knows works.
        constexpr std::array<ConfigRename, 1> kPretendRenames{
                ConfigRename{.to = 2, .section = "typing", .was = "stop_at_error", .now = "stop_on_error"},
        };

        class ConfigMigrationTest : public ::testing::Test {
        protected:
            [[nodiscard]] std::filesystem::path file() const { return env_.config() / "config.toml"; }

            void write(std::string_view contents) const { std::ofstream{file()} << contents; }

            [[nodiscard]] std::string read() const { return read(file()); }

            [[nodiscard]] static std::string read(const std::filesystem::path& path) {
                const std::ifstream in{path};
                std::ostringstream text;
                text << in.rdbuf();
                return text.str();
            }

            testing::TempEnv env_;
        };

        // ---- the shipped table ---------------------------------------------

        TEST_F(ConfigMigrationTest, TwoPointZeroShipsAtConfigVersionOneWithNothingToMigrate) {
            EXPECT_EQ(latest_config_version(), 1);
            EXPECT_TRUE(config_renames().empty()) << "nothing has been renamed yet, and saying so is the point";
        }

        TEST_F(ConfigMigrationTest, AVersionOneFileIsLeftExactlyAsItIs) {
            constexpr std::string_view original = "config_version = 1\n\n[general]\ndefault_duration_s = 30\n";
            write(original);

            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file());

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->from, 1);
            EXPECT_EQ(outcome->to, 1);
            EXPECT_EQ(read(), original) << "byte for byte";
            EXPECT_FALSE(std::filesystem::exists(file().string() + ".bak")) << "and nothing to back up";
        }

        TEST_F(ConfigMigrationTest, AFileWithNoVersionKeyIsAVersionOneFile) {
            // Version 1 is the shape that had no version key yet, so its
            // absence is information rather than an error.
            write("[general]\ndefault_duration_s = 30\n");

            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file());

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->from, 1);
        }

        TEST_F(ConfigMigrationTest, AMissingFileIsNothingToMigrate) {
            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file());

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->to, latest_config_version());
            EXPECT_FALSE(std::filesystem::exists(file())) << "and no file is invented for one";
        }

        // ---- migrating -----------------------------------------------------

        TEST_F(ConfigMigrationTest, ARenamedKeyKeepsItsValue) {
            // The whole point. The person who set that value meant it.
            write("config_version = 1\n\n[typing]\nstop_at_error = \"word\"\n");

            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file(), kPretendRenames, 2);

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->from, 1);
            EXPECT_EQ(outcome->to, 2);
            EXPECT_NE(read().find("stop_on_error = \"word\""), std::string::npos) << read();
            EXPECT_EQ(read().find("stop_at_error"), std::string::npos) << "the old name is gone";
            EXPECT_NE(read().find("config_version = 2"), std::string::npos) << read();
        }

        TEST_F(ConfigMigrationTest, TheBackupIsWrittenBeforeAnythingIsChanged) {
            constexpr std::string_view original = "config_version = 1\n\n[typing]\nstop_at_error = \"word\"\n";
            write(original);

            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file(), kPretendRenames, 2);

            ASSERT_TRUE(outcome);
            ASSERT_FALSE(outcome->backup.empty());
            EXPECT_EQ(outcome->backup, file().string() + ".bak");
            EXPECT_EQ(read(outcome->backup), original) << "the file as the user had it";
        }

        TEST_F(ConfigMigrationTest, TheUserIsToldWhatChanged) {
            // A renamed key should be a thing that happened, not a thing that
            // vanished.
            write("config_version = 1\n\n[typing]\nstop_at_error = \"word\"\n");

            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file(), kPretendRenames, 2);

            ASSERT_TRUE(outcome);
            ASSERT_EQ(outcome->notes.size(), 1U);
            EXPECT_NE(outcome->notes.front().find("stop_at_error"), std::string::npos) << outcome->notes.front();
            EXPECT_NE(outcome->notes.front().find("stop_on_error"), std::string::npos) << outcome->notes.front();
        }

        TEST_F(ConfigMigrationTest, MigratingIsIdempotent) {
            write("config_version = 1\n\n[typing]\nstop_at_error = \"word\"\n");
            ASSERT_TRUE(migrate_config_file(file(), kPretendRenames, 2));
            const std::string once = read();

            const Result<ConfigMigrationOutcome> again = migrate_config_file(file(), kPretendRenames, 2);

            ASSERT_TRUE(again) << (again ? "" : again.error().context);
            EXPECT_EQ(again->from, 2);
            EXPECT_EQ(again->to, 2);
            EXPECT_TRUE(again->notes.empty());
            EXPECT_EQ(read(), once) << "running it twice changes nothing the second time";
        }

        TEST_F(ConfigMigrationTest, CommentsAndOrderingSurvive) {
            // The file is a document somebody wrote. Round-tripping it through
            // a TOML tree would hand it back sorted and stripped of comments,
            // which is why the rename is textual.
            write(R"(config_version = 1

# I like a long run.
[general]
default_duration_s = 120   # two minutes

[typing]
stop_at_error = "letter"   # I want to be told immediately
)");

            ASSERT_TRUE(migrate_config_file(file(), kPretendRenames, 2));

            const std::string migrated = read();
            EXPECT_NE(migrated.find("# I like a long run."), std::string::npos) << migrated;
            EXPECT_NE(migrated.find("# I want to be told immediately"), std::string::npos) << migrated;
            EXPECT_LT(migrated.find("[general]"), migrated.find("[typing]")) << "and in the order they wrote it";
        }

        TEST_F(ConfigMigrationTest, AMigratedFileStillLoads) {
            // The end-to-end claim: after a rename, the value the user set is
            // the value the program uses.
            write("config_version = 1\n\n[typing]\nstop_at_error = \"word\"\n");
            ASSERT_TRUE(migrate_config_file(file(), kPretendRenames, 2));

            TomlConfigStore store{file()};
            const Result<app::LoadedConfig> loaded = store.load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.typing.stop_on_error, core::StopOnError::Word) << "the setting survived";
        }

        // ---- the future ----------------------------------------------------

        TEST_F(ConfigMigrationTest, AFutureVersionIsLeftAloneAndReported) {
            // A newer TypeIt wrote it. Refusing to start because the settings
            // are too new would be worse than starting with some ignored — and
            // rewriting them would destroy what that newer version needs.
            constexpr std::string_view original = "config_version = 99\n\n[general]\nsomething_new = true\n";
            write(original);

            const Result<ConfigMigrationOutcome> outcome = migrate_config_file(file());

            ASSERT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
            EXPECT_EQ(outcome->from, 99);
            EXPECT_EQ(outcome->to, 99) << "not downgraded";
            EXPECT_EQ(read(), original) << "and not rewritten";
            ASSERT_EQ(outcome->notes.size(), 1U);
            EXPECT_NE(outcome->notes.front().find("newer version"), std::string::npos) << outcome->notes.front();
            EXPECT_TRUE(outcome->backup.empty()) << "nothing was changed, so nothing was backed up";
        }

    }  // namespace
}  // namespace typeit::infra
