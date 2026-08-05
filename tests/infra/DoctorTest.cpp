// The report, over installs that are healthy and installs that are not.
//
// The point of every case here is that nothing is an error. A missing
// database, an unwritable directory and a corrupt file are *findings*: a
// diagnostic that refuses to run because something is wrong is a diagnostic
// that is never there when it is needed.

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "typeit/app/Capabilities.h"
#include "typeit/core/Version.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/Doctor.h"
#include "typeit/infra/db/Migrator.h"
#include "typeit/infra/db/Schema.h"
#include "typeit/infra/db/SqliteDatabase.h"
#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/infra/term/Capabilities.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        /// An environment that is exactly what a test says it is. The real one
        /// would make every case depend on the machine it runs on.
        Environment environment_of(std::map<std::string, std::string> variables) {
            return [variables = std::move(variables)](std::string_view name) -> std::optional<std::string> {
                const auto found = variables.find(std::string{name});
                if (found == variables.end() || found->second.empty()) {
                    return std::nullopt;
                }
                return found->second;
            };
        }

        /// One `key: value` line of the rendered report, or empty.
        std::string field(std::string_view text, std::string_view key) {
            const std::string needle = "\n" + std::string{key} + ": ";
            const std::size_t at = text.find(needle);
            if (at == std::string_view::npos) {
                return {};
            }
            const std::size_t from = at + needle.size();
            return std::string{text.substr(from, text.find('\n', from) - from)};
        }

        class DoctorTest : public ::testing::Test {
        protected:
            [[nodiscard]] Examination an_examination(std::map<std::string, std::string> extra = {}) const {
                // The overrides are applied on top of a resolved base, so the
                // platform's own variables still have to be there. All three
                // are set so the fixture reads the same on either platform.
                extra.try_emplace("HOME", env_.root().string());
                extra.try_emplace("APPDATA", env_.root().string());
                extra.try_emplace("LOCALAPPDATA", env_.root().string());
                extra.try_emplace("TYPEIT_CONFIG_DIR", env_.config().string());
                extra.try_emplace("TYPEIT_DATA_DIR", env_.data().string());
                return Examination{
                        .environment = environment_of(std::move(extra)),
                        .executable = env_.root() / "bin" / "typeit",
                        .working_directory = env_.root(),
                        .install_prefix = env_.root() / "usr",
                };
            }

            [[nodiscard]] std::filesystem::path database_path() const { return env_.data() / "typeit.db"; }

            void given_a_healthy_database() const {
                std::filesystem::create_directories(env_.data());
                core::Result<SqliteDatabase> database =
                        SqliteDatabase::open(database_path(), SqliteDatabase::OpenMode::CreateIfMissing);
                ASSERT_TRUE(database) << (database ? "" : database.error().context);
                ASSERT_TRUE(migrate_to_latest(*database));
            }

            testing::TempEnv env_;
        };

        // --- Capability detection, table-driven over UX section 6.2 ---------

        TEST(CapabilityDetectionTest, NoColorBeatsEverything) {
            const app::Capabilities detected =
                    detect_capabilities(environment_of({{"NO_COLOR", "1"}, {"COLORTERM", "truecolor"}}));

            EXPECT_EQ(detected.color, app::ColorDepth::Mono);
            EXPECT_EQ(detected.glyphs, app::GlyphSet::Ascii);
            EXPECT_EQ(detected.reason, "NO_COLOR is set");
        }

        TEST(CapabilityDetectionTest, TheOverridesWin) {
            const app::Capabilities colored = detect_capabilities(
                    environment_of({{"TYPEIT_COLOR", "256"}, {"COLORTERM", "truecolor"}, {"TERM", "dumb"}}));
            EXPECT_EQ(colored.color, app::ColorDepth::Ansi256);
            EXPECT_EQ(colored.reason, "TYPEIT_COLOR=256");

            const app::Capabilities glyphed =
                    detect_capabilities(environment_of({{"TYPEIT_GLYPHS", "unicode"}, {"TERM", "dumb"}}));
            EXPECT_EQ(glyphed.glyphs, app::GlyphSet::Unicode) << "a font that can draw a box is not a colour question";
            EXPECT_EQ(glyphed.color, app::ColorDepth::Ansi16);
            EXPECT_NE(glyphed.reason.find("TYPEIT_GLYPHS=unicode"), std::string::npos);
        }

        TEST(CapabilityDetectionTest, AnOverrideNobodyRecognisesIsIgnoredRatherThanObeyed) {
            const app::Capabilities detected =
                    detect_capabilities(environment_of({{"TYPEIT_COLOR", "chartreuse"}, {"COLORTERM", "truecolor"}}));

            EXPECT_EQ(detected.color, app::ColorDepth::TrueColor);
            EXPECT_EQ(detected.reason, "COLORTERM=truecolor");
        }

        TEST(CapabilityDetectionTest, ColorTermMeansTrueColorInBothSpellings) {
            EXPECT_EQ(detect_capabilities(environment_of({{"COLORTERM", "truecolor"}})).color,
                      app::ColorDepth::TrueColor);
            EXPECT_EQ(detect_capabilities(environment_of({{"COLORTERM", "24bit"}})).color, app::ColorDepth::TrueColor);
            EXPECT_EQ(detect_capabilities(environment_of({{"COLORTERM", "24bit"}})).reason, "COLORTERM=24bit");
        }

        TEST(CapabilityDetectionTest, WindowsTerminalIsRecognisedByItsOwnVariable) {
            // It reports neither COLORTERM nor a useful TERM, and does both.
            const app::Capabilities detected = detect_capabilities(environment_of({{"WT_SESSION", "abc-123"}}));

            EXPECT_EQ(detected.color, app::ColorDepth::TrueColor);
            EXPECT_EQ(detected.glyphs, app::GlyphSet::Unicode);
            EXPECT_NE(detected.reason.find("WT_SESSION"), std::string::npos);
        }

        TEST(CapabilityDetectionTest, A256ColorTermIsRecognisedAnywhereInTheName) {
            const app::Capabilities detected = detect_capabilities(environment_of({{"TERM", "screen-256color"}}));

            EXPECT_EQ(detected.color, app::ColorDepth::Ansi256);
            EXPECT_EQ(detected.reason, "TERM=screen-256color");
        }

        TEST(CapabilityDetectionTest, TheConsoleAndTheDumbTerminalGetTheFloor) {
            for (const std::string_view name: {"linux", "dumb"}) {
                const app::Capabilities detected = detect_capabilities(environment_of({{"TERM", std::string{name}}}));
                EXPECT_EQ(detected.color, app::ColorDepth::Ansi16) << name;
                EXPECT_EQ(detected.glyphs, app::GlyphSet::Ascii) << name;
            }
        }

        TEST(CapabilityDetectionTest, AnUnknownTerminalGetsSomethingThatCertainlyWorks) {
            const app::Capabilities nothing = detect_capabilities(environment_of({}));
            EXPECT_EQ(nothing.color, app::ColorDepth::Ansi16);
            EXPECT_EQ(nothing.glyphs, app::GlyphSet::Ascii);
            EXPECT_EQ(nothing.reason, "neither COLORTERM nor TERM is set");

            const app::Capabilities unknown = detect_capabilities(environment_of({{"TERM", "vt52"}}));
            EXPECT_EQ(unknown.color, app::ColorDepth::Ansi16);
            EXPECT_NE(unknown.reason.find("vt52"), std::string::npos);
        }

        TEST(CapabilityDetectionTest, AVariableSetToNothingIsAVariableNobodySet) {
            // What the whole Environment port means by "set", and what
            // no-color.org itself says about an empty NO_COLOR.
            const app::Capabilities detected =
                    detect_capabilities(environment_of({{"NO_COLOR", ""}, {"COLORTERM", "truecolor"}}));

            EXPECT_EQ(detected.color, app::ColorDepth::TrueColor);
        }

        TEST(CapabilityDetectionTest, EveryValueHasAStableSpelling) {
            // These names go into `--doctor` output and into the config file.
            EXPECT_EQ(to_string(app::ColorDepth::Mono), "mono");
            EXPECT_EQ(to_string(app::ColorDepth::Ansi16), "16");
            EXPECT_EQ(to_string(app::ColorDepth::Ansi256), "256");
            EXPECT_EQ(to_string(app::ColorDepth::TrueColor), "truecolor");
            EXPECT_EQ(to_string(app::GlyphSet::Ascii), "ascii");
            EXPECT_EQ(to_string(app::GlyphSet::Unicode), "unicode");
        }

        // --- The report -----------------------------------------------------

        TEST_F(DoctorTest, AHealthyInstallReportsItself) {
            given_a_healthy_database();

            const DoctorReport report = diagnose(an_examination({{"COLORTERM", "truecolor"}}));

            EXPECT_EQ(report.version, kVersionString);
            EXPECT_EQ(report.capabilities.color, app::ColorDepth::TrueColor);
            EXPECT_TRUE(report.database.exists);
            ASSERT_TRUE(report.database.schema_version.has_value());
            EXPECT_EQ(*report.database.schema_version, report.database.understood_version);
            EXPECT_EQ(report.database.integrity, "ok");
            EXPECT_EQ(report.database.sessions, 0);
            EXPECT_FALSE(report.sqlite_version.empty());
        }

        TEST_F(DoctorTest, AMissingDatabaseIsAFindingRatherThanAFailure) {
            // A first run has no database, and saying "missing" is more use
            // than an error about a file nobody has made yet.
            const DoctorReport report = diagnose(an_examination());

            EXPECT_FALSE(report.database.exists);
            EXPECT_FALSE(report.database.schema_version.has_value());
            EXPECT_EQ(report.database.integrity, "no database yet");
            EXPECT_GT(report.database.understood_version, 0) << "what this binary would create";
        }

        TEST_F(DoctorTest, AskingAboutAMissingDatabaseDoesNotCreateOne) {
            // `--doctor` on a typo in --data-dir must not leave an empty
            // database behind to confuse the next run.
            const DoctorReport report = diagnose(an_examination());

            EXPECT_FALSE(std::filesystem::exists(report.database.path));
        }

        TEST_F(DoctorTest, ACorruptDatabaseIsReportedRatherThanCrashedOn) {
            std::filesystem::create_directories(env_.data());
            {
                std::ofstream file{database_path(), std::ios::binary};
                file << "this is not a database, it is a sentence";
            }

            const DoctorReport report = diagnose(an_examination());

            EXPECT_TRUE(report.database.exists);
            EXPECT_NE(report.database.integrity, "ok");
            EXPECT_FALSE(report.database.integrity.empty()) << "and it says what happened";
        }

        TEST_F(DoctorTest, AMissingConfigDirectoryIsReportedAsMissing) {
            // A first run has no config directory, and `--doctor` on a typo in
            // `--config` must say so rather than report a healthy install
            // somewhere nobody meant.
            const DoctorReport report =
                    diagnose(an_examination({{"TYPEIT_CONFIG_DIR", (env_.root() / "nowhere").string()}}));

            const auto config = std::ranges::find(report.paths, "config", &PathReport::label);
            ASSERT_NE(config, report.paths.end());
            EXPECT_FALSE(config->exists);
            EXPECT_FALSE(config->writable);
        }

        TEST_F(DoctorTest, AWritableDirectoryIsProvedByWritingInIt) {
            const DoctorReport report = diagnose(an_examination());

            const auto config = std::ranges::find(report.paths, "config", &PathReport::label);
            ASSERT_NE(config, report.paths.end());
            EXPECT_TRUE(config->exists);
            EXPECT_TRUE(config->writable);
        }

        TEST_F(DoctorTest, TheProbeFileIsCleanedUpAfterItself) {
            // A diagnostic that leaves litter behind gets a bug report of its
            // own.
            const DoctorReport report = diagnose(an_examination());
            ASSERT_FALSE(report.paths.empty());

            EXPECT_TRUE(std::filesystem::is_empty(env_.config()));
        }

        TEST_F(DoctorTest, MissingAssetsAreNamedRatherThanOmitted) {
            const DoctorReport report = diagnose(an_examination());

            const auto assets = std::ranges::find(report.paths, "assets", &PathReport::label);
            ASSERT_NE(assets, report.paths.end());
            EXPECT_FALSE(assets->exists);
            EXPECT_FALSE(assets->problem.empty()) << "and every place that was looked in";
        }

        // --- The rendering --------------------------------------------------

        TEST_F(DoctorTest, TheOutputIsStableAcrossRuns) {
            // Nothing dated in it, so two runs on one machine differ only where
            // the machine differs — which is what makes it pasteable into an
            // issue.
            given_a_healthy_database();
            const Examination examination = an_examination({{"TERM", "xterm-256color"}});

            EXPECT_EQ(to_text(diagnose(examination)), to_text(diagnose(examination)));
        }

        TEST_F(DoctorTest, TheOutputIsParseableLineByLine) {
            given_a_healthy_database();

            const std::string text = to_text(diagnose(an_examination({{"COLORTERM", "truecolor"}})));

            EXPECT_TRUE(text.starts_with("typeit: ")) << text;
            EXPECT_NE(text.find("\n[terminal]\n"), std::string::npos) << text;
            EXPECT_NE(text.find("\n[paths]\n"), std::string::npos) << text;
            EXPECT_NE(text.find("\n[database]\n"), std::string::npos) << text;
            EXPECT_NE(text.find("\n[dependencies]\n"), std::string::npos) << text;

            EXPECT_EQ(field(text, "color"), "truecolor");
            EXPECT_EQ(field(text, "glyphs"), "unicode");
            EXPECT_EQ(field(text, "reason"), "COLORTERM=truecolor");
            EXPECT_EQ(field(text, "integrity"), "ok");
            EXPECT_EQ(field(text, "sessions"), "0");
            EXPECT_EQ(field(text, "understood"), std::to_string(latest_schema_version()));
        }

        TEST_F(DoctorTest, TheRenderedReportSaysWhichDirectoriesAreUsable) {
            const std::string text =
                    to_text(diagnose(an_examination({{"TYPEIT_CONFIG_DIR", (env_.root() / "nowhere").string()}})));

            EXPECT_NE(text.find("(exists, writable)"), std::string::npos) << text;
            EXPECT_NE(text.find("(missing, not writable)"), std::string::npos) << text;
        }

        TEST_F(DoctorTest, TheVersionIsReadRatherThanSpelled) {
            const std::string text = to_text(diagnose(an_examination()));

            EXPECT_EQ(field("\n" + text, "typeit"), kVersionString);
        }

    }  // namespace
}  // namespace typeit::infra
