#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/config/TomlConfigStore.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        using core::Config;
        using core::ErrorCode;
        using core::Result;
        using core::Status;

        class ConfigStoreTest : public ::testing::Test {
        protected:
            [[nodiscard]] std::filesystem::path file() const { return env_.config() / "config.toml"; }

            void write(std::string_view contents) const { std::ofstream{file()} << contents; }

            [[nodiscard]] std::string read() const {
                const std::ifstream in{file()};
                std::ostringstream text;
                text << in.rdbuf();
                return text.str();
            }

            [[nodiscard]] TomlConfigStore store() const { return TomlConfigStore{file()}; }

            /// Every warning joined, for a test that only cares that the key
            /// was named.
            static std::string warnings_of(const app::LoadedConfig& loaded) {
                std::string all;
                for (const std::string& warning: loaded.warnings) {
                    all += warning + "\n";
                }
                return all;
            }

            testing::TempEnv env_;
        };

        // ---- first run -----------------------------------------------------

        TEST_F(ConfigStoreTest, AMissingFileYieldsTheDefaultsAndWritesATemplate) {
            TomlConfigStore config_store = store();

            const Result<app::LoadedConfig> loaded = config_store.load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.general.default_duration_s, Config{}.general.default_duration_s);
            EXPECT_TRUE(std::filesystem::exists(file())) << "and the file now exists to be edited";
            EXPECT_NE(read().find("# TypeIt configuration"), std::string::npos) << "with its comments";
        }

        TEST_F(ConfigStoreTest, TheTemplateItWritesLoadsBackWithNoWarnings) {
            // The strongest thing to say about a generated file: it is a file
            // this program would accept from a user.
            TomlConfigStore config_store = store();
            ASSERT_TRUE(config_store.load());

            const Result<app::LoadedConfig> reloaded = config_store.load();

            ASSERT_TRUE(reloaded) << (reloaded ? "" : reloaded.error().context);
            EXPECT_TRUE(reloaded->warnings.empty()) << warnings_of(*reloaded);
        }

        // ---- loading -------------------------------------------------------

        TEST_F(ConfigStoreTest, ACompleteFileLoadsEveryField) {
            write(R"(
config_version = 1

[general]
default_mode = "quote"
default_duration_s = 120
default_word_count = 25
countdown_s = 0
confirm_quit = false
log_level = "debug"

[appearance]
theme = "typeit-light"
color_depth = "256"
glyphs = "ascii"
caret = "underline"
caret_blink = true
layout = "compact"
line_width = 72
lines_visible = 5
show_live_wpm = false

[typing]
stop_on_error = "word"
allow_backspace = false
strict_spaces = false
space_advances_word = false
blind_mode = true
confidence_mode = "max"

[text]
tab_width = 8
chunk_graphemes = 500

[race]
preset = "brutal"
start_wpm = 45
ramp_up = 1.25
min_accuracy = 0.8
lead_comfort = 40
lead_danger = 12

[history]
keep_keystroke_logs = true
retention_days = 90

[network]
enabled = true
timeout_s = 15
)");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_TRUE(loaded->warnings.empty()) << warnings_of(*loaded);
            EXPECT_EQ(loaded->config.general.default_mode, "quote");
            EXPECT_EQ(loaded->config.general.default_duration_s, 120);
            EXPECT_EQ(loaded->config.general.countdown_s, 0);
            EXPECT_FALSE(loaded->config.general.confirm_quit);
            EXPECT_EQ(loaded->config.appearance.line_width, 72);
            EXPECT_TRUE(loaded->config.appearance.caret_blink);
            EXPECT_FALSE(loaded->config.appearance.show_live_wpm);
            EXPECT_EQ(loaded->config.typing.stop_on_error, core::StopOnError::Word);
            EXPECT_EQ(loaded->config.typing.confidence_mode, core::ConfidenceMode::Max);
            EXPECT_FALSE(loaded->config.typing.allow_backspace);
            EXPECT_TRUE(loaded->config.typing.blind_mode);
            EXPECT_EQ(loaded->config.text.tab_width, 8);
            EXPECT_EQ(loaded->config.race.preset, "brutal");
            EXPECT_DOUBLE_EQ(loaded->config.race.ramp_up, 1.25);
            EXPECT_EQ(loaded->config.history.retention_days, 90);
            EXPECT_TRUE(loaded->config.network.enabled);
        }

        TEST_F(ConfigStoreTest, APartialFileTakesTheRestFromTheDefaults) {
            write("[general]\ndefault_duration_s = 15\n");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.general.default_duration_s, 15);
            EXPECT_EQ(loaded->config.general.default_mode, Config{}.general.default_mode);
            EXPECT_EQ(loaded->config.appearance.theme, Config{}.appearance.theme);
        }

        TEST_F(ConfigStoreTest, TheKeysAndConvertersTablesLoadWhole) {
            write(R"(
[keys]
quit = "ctrl-q"
help = "f1"

[import.converters]
"application/pdf" = "pdftotext {input} -"
)");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_TRUE(loaded->warnings.empty()) << warnings_of(*loaded);
            EXPECT_EQ(loaded->config.keys.at("quit"), "ctrl-q");
            EXPECT_EQ(loaded->config.import_.converters.at("application/pdf"), "pdftotext {input} -");
        }

        // ---- complaints ----------------------------------------------------

        TEST_F(ConfigStoreTest, AnUnknownKeyWarnsAndDoesNotFailTheLoad) {
            // Forward compatibility: a file written by a newer TypeIt still
            // loads on this one.
            write("[general]\ndefault_duration_s = 30\nfavourite_colour = \"blue\"\n");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.general.default_duration_s, 30) << "the rest of the file still applies";
            EXPECT_NE(warnings_of(*loaded).find("general.favourite_colour"), std::string::npos) << warnings_of(*loaded);
        }

        TEST_F(ConfigStoreTest, AnOutOfRangeValueFallsBackToItsDefaultAndSaysWhich) {
            write("[general]\ndefault_duration_s = 99999\n");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.general.default_duration_s, Config{}.general.default_duration_s);
            EXPECT_NE(warnings_of(*loaded).find("general.default_duration_s"), std::string::npos)
                    << warnings_of(*loaded);
            EXPECT_NE(warnings_of(*loaded).find("1..3600"), std::string::npos)
                    << "and what would have worked: " << warnings_of(*loaded);
        }

        TEST_F(ConfigStoreTest, AValueOfTheWrongTypeFallsBackAndSaysSo) {
            write("[general]\ndefault_duration_s = \"thirty\"\n");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.general.default_duration_s, Config{}.general.default_duration_s);
            EXPECT_NE(warnings_of(*loaded).find("wrong type"), std::string::npos) << warnings_of(*loaded);
        }

        TEST_F(ConfigStoreTest, AnUnknownEnumValueFallsBackAndNamesTheValidSet) {
            write("[typing]\nstop_on_error = \"sometimes\"\n");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.typing.stop_on_error, core::StopOnError::Off);
            EXPECT_NE(warnings_of(*loaded).find("off, letter, word"), std::string::npos) << warnings_of(*loaded);
        }

        TEST_F(ConfigStoreTest, OneBadValueDoesNotCostTheRestOfTheFile) {
            write("[general]\ndefault_duration_s = 99999\ndefault_word_count = 25\n");

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.general.default_duration_s, Config{}.general.default_duration_s);
            EXPECT_EQ(loaded->config.general.default_word_count, 25);
        }

        // ---- the file the user wrote ---------------------------------------

        TEST_F(ConfigStoreTest, AMalformedFileIsReportedWithALineAndNeverOverwritten) {
            // The most important test in this file. Somebody hand-wrote that
            // config; losing it because we could not parse it would be the
            // worst thing this class could do.
            constexpr std::string_view broken = "[general\ndefault_duration_s = 30\n";
            write(broken);

            const Result<app::LoadedConfig> loaded = store().load();

            ASSERT_FALSE(loaded);
            EXPECT_EQ(loaded.error().code, ErrorCode::ConfigParse);
            EXPECT_NE(loaded.error().context.find(":1:"), std::string::npos)
                    << "with the line and column: " << loaded.error().context;
            EXPECT_EQ(read(), broken) << "and the file is exactly as the user left it";
        }

        TEST_F(ConfigStoreTest, AMalformedFileIsNotReplacedByATemplateEither) {
            write("this is not toml at all");

            ASSERT_FALSE(store().load());

            EXPECT_EQ(read(), "this is not toml at all");
        }

        // ---- saving --------------------------------------------------------

        TEST_F(ConfigStoreTest, SaveThenLoadRoundTrips) {
            Config config;
            config.general.default_mode = "words";
            config.general.default_duration_s = 45;
            config.typing.stop_on_error = core::StopOnError::Letter;
            config.typing.confidence_mode = core::ConfidenceMode::On;
            config.race.min_accuracy = 0.75;
            config.keys["quit"] = "ctrl-c";
            config.import_.converters["*"] = "pandoc {input}";

            TomlConfigStore config_store = store();
            ASSERT_TRUE(config_store.save(config));
            const Result<app::LoadedConfig> loaded = config_store.load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_TRUE(loaded->warnings.empty()) << warnings_of(*loaded);
            EXPECT_EQ(loaded->config.general.default_mode, "words");
            EXPECT_EQ(loaded->config.general.default_duration_s, 45);
            EXPECT_EQ(loaded->config.typing.stop_on_error, core::StopOnError::Letter);
            EXPECT_EQ(loaded->config.typing.confidence_mode, core::ConfidenceMode::On);
            EXPECT_DOUBLE_EQ(loaded->config.race.min_accuracy, 0.75);
            EXPECT_EQ(loaded->config.keys.at("quit"), "ctrl-c");
            EXPECT_EQ(loaded->config.import_.converters.at("*"), "pandoc {input}");
        }

        TEST_F(ConfigStoreTest, NonAsciiValuesRoundTrip) {
            Config config;
            config.appearance.theme = "tema-čšž-漢字";

            TomlConfigStore config_store = store();
            ASSERT_TRUE(config_store.save(config));

            const Result<app::LoadedConfig> loaded = config_store.load();
            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.appearance.theme, "tema-čšž-漢字");
        }

        TEST_F(ConfigStoreTest, AValueContainingAQuoteRoundTrips) {
            Config config;
            config.appearance.theme = R"(say "what" \ then)";

            TomlConfigStore config_store = store();
            ASSERT_TRUE(config_store.save(config));

            const Result<app::LoadedConfig> loaded = config_store.load();
            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.appearance.theme, R"(say "what" \ then)");
        }

        TEST_F(ConfigStoreTest, SavingLeavesNoTemporaryFileBehind) {
            TomlConfigStore config_store = store();

            ASSERT_TRUE(config_store.save(Config{}));

            EXPECT_FALSE(std::filesystem::exists(file().string() + ".tmp"))
                    << "the temporary is renamed over the target, not left beside it";
        }

        TEST_F(ConfigStoreTest, SavingOverAnExistingFileReplacesItWhole) {
            write("[general]\ndefault_duration_s = 15\n");
            Config config;
            config.general.default_duration_s = 60;

            TomlConfigStore config_store = store();
            ASSERT_TRUE(config_store.save(config));

            const Result<app::LoadedConfig> loaded = config_store.load();
            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.general.default_duration_s, 60);
            EXPECT_EQ(read().find("15"), std::string::npos) << "no fragment of the old file survives";
        }

#ifndef _WIN32
        TEST_F(ConfigStoreTest, AReadOnlyDirectoryIsAClearError) {
            const std::filesystem::path locked = env_.root() / "locked";
            std::filesystem::create_directories(locked);
            std::filesystem::permissions(locked,
                                         std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);

            TomlConfigStore config_store{locked / "config.toml"};
            const Status saved = config_store.save(Config{});

            std::filesystem::permissions(locked, std::filesystem::perms::owner_all);

            ASSERT_FALSE(saved);
            EXPECT_NE(saved.error().context.find("config.toml"), std::string::npos) << saved.error().context;
        }
#endif

    }  // namespace
}  // namespace typeit::infra
