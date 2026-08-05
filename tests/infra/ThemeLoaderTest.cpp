// Theme files, and everything a theme author can get wrong.
//
// The rule under test throughout: a theme with a mistake in it still loads.
// Refusing to start over a typo in one hex value would be a worse answer than
// drawing that one element in the default and saying so — the author is
// looking at the screen, and the screen is where they will see it.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "typeit/app/Theme.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/theme/ThemeLoader.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        LoadedTheme parsed(std::string_view text) {
            const core::Result<LoadedTheme> loaded = parse_theme(text);
            EXPECT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            return loaded.value_or(LoadedTheme{});
        }

        /// Whether any warning mentions `fragment`, so a test can say what it
        /// expects to be told without pinning the whole sentence.
        bool warned_about(const LoadedTheme& loaded, std::string_view fragment) {
            return std::ranges::any_of(loaded.warnings, [fragment](const std::string& warning) {
                return warning.find(fragment) != std::string::npos;
            });
        }

        /// Where the shipped themes are in the source tree. Compiled in rather
        /// than found at runtime — this is a test of the files, and it must
        /// fail on the machine that edits them (defect C2 stays fixed because
        /// nothing *shipped* does this).
        std::filesystem::path shipped_themes() { return std::filesystem::path{TYPEIT_ASSETS_DIR} / "themes"; }

        constexpr std::string_view kMinimal = R"(
name = "test"
[colors]
text_correct = "#010203"
)";

        // --- The shipped themes -----------------------------------------------

        TEST(ThemeLoaderTest, EveryBuiltInThemeShipsAndLoads) {
            for (const std::string& name: built_in_theme_names()) {
                const core::Result<LoadedTheme> loaded = load_theme(shipped_themes() / (name + ".toml"));

                ASSERT_TRUE(loaded) << name << ": " << (loaded ? "" : loaded.error().context);
                EXPECT_EQ(loaded->theme.name, name) << "the file's name must match its filename";
            }
        }

        TEST(ThemeLoaderTest, EveryBuiltInThemeDefinesEverySemanticColour) {
            // Not "loads without error" — a theme that omitted half its colours
            // would load, take the defaults, and look like the default theme.
            for (const std::string& name: built_in_theme_names()) {
                const core::Result<LoadedTheme> loaded = load_theme(shipped_themes() / (name + ".toml"));
                ASSERT_TRUE(loaded) << name;

                EXPECT_TRUE(loaded->warnings.empty())
                        << name << " warns: " << (loaded->warnings.empty() ? "" : loaded->warnings.front());
            }
        }

        TEST(ThemeLoaderTest, TheFourNamedInTheAcceptanceAllShip) {
            const std::vector<std::string> names = built_in_theme_names();

            for (const std::string_view wanted: {"typeit-dark", "typeit-light", "high-contrast", "mono"}) {
                EXPECT_NE(std::ranges::find(names, wanted), names.end()) << wanted;
                EXPECT_TRUE(std::filesystem::exists(shipped_themes() / (std::string{wanted} + ".toml"))) << wanted;
            }
        }

        // --- Parsing ------------------------------------------------------------

        TEST(ThemeLoaderTest, AColourIsReadAsWritten) {
            const LoadedTheme loaded = parsed(kMinimal);

            EXPECT_EQ(loaded.theme.name, "test");
            EXPECT_EQ(loaded.theme.color(app::ThemeColor::TextCorrect),
                      (app::Rgb{.red = 0x01, .green = 0x02, .blue = 0x03}));
        }

        TEST(ThemeLoaderTest, AMissingColourTakesTheDefaultAndWarns) {
            const LoadedTheme loaded = parsed(kMinimal);

            EXPECT_EQ(loaded.theme.color(app::ThemeColor::Caret), app::Theme{}.color(app::ThemeColor::Caret));
            EXPECT_TRUE(warned_about(loaded, "colors.caret")) << "and it says which one";
        }

        TEST(ThemeLoaderTest, AnInvalidHexValueIsRejectedWithTheKeyNamed) {
            const LoadedTheme loaded = parsed(R"(
[colors]
text_correct = "#ff00zz"
)");

            EXPECT_TRUE(warned_about(loaded, "colors.text_correct"));
            EXPECT_TRUE(warned_about(loaded, "#rrggbb")) << "and what was expected";
            EXPECT_EQ(loaded.theme.color(app::ThemeColor::TextCorrect),
                      app::Theme{}.color(app::ThemeColor::TextCorrect))
                    << "and the default stands";
        }

        TEST(ThemeLoaderTest, AShortHexFormIsNotAcceptedAsASecondSpelling) {
            // A loader that took both would invite files that only work on one
            // of them.
            const LoadedTheme loaded = parsed(R"(
[colors]
caret = "#fff"
)");

            EXPECT_TRUE(warned_about(loaded, "colors.caret"));
        }

        TEST(ThemeLoaderTest, AnUnknownKeyIsIgnoredRatherThanRefused) {
            // Forward compatibility: a theme written for a later version should
            // still load on this one.
            const LoadedTheme loaded = parsed(R"(
name = "future"
[colors]
text_correct = "#010203"
hyperspace   = "#ff00ff"
)");

            EXPECT_EQ(loaded.theme.name, "future");
            EXPECT_TRUE(warned_about(loaded, "hyperspace"));
            EXPECT_TRUE(warned_about(loaded, "ignored"));
        }

        TEST(ThemeLoaderTest, AFileThatIsNotTomlIsTheOneFatalCase) {
            // The one case where continuing would mean rendering a theme whose
            // author cannot see the effect of what they wrote.
            const core::Result<LoadedTheme> loaded = parse_theme("this is not = = toml");

            ASSERT_FALSE(loaded);
            EXPECT_EQ(loaded.error().code, core::ErrorCode::ConfigParse);
            EXPECT_NE(loaded.error().context.find("line"), std::string::npos) << loaded.error().context;
        }

        TEST(ThemeLoaderTest, AThemeWithNoNameWarnsBecauseItCannotBeSelected) {
            const LoadedTheme loaded = parsed(R"(
[colors]
caret = "#010203"
)");

            EXPECT_TRUE(warned_about(loaded, "name"));
        }

        TEST(ThemeLoaderTest, ANewerThemeVersionWarnsRatherThanFailing) {
            // VERSIONING §5: unknown keys are ignored, so most theme changes
            // need no bump — a file that does need something newer says so
            // instead of quietly rendering wrong.
            const LoadedTheme loaded = parsed(R"(
theme_version = 99
name = "future"
[colors]
caret = "#010203"
)");

            EXPECT_EQ(loaded.theme.name, "future");
            EXPECT_TRUE(warned_about(loaded, "theme_version = 99"));
        }

        TEST(ThemeLoaderTest, AMatchingThemeVersionIsSilent) {
            const LoadedTheme loaded = parsed(R"(
theme_version = 1
[colors]
caret = "#010203"
)");

            EXPECT_FALSE(warned_about(loaded, "theme_version"));
        }

        TEST(ThemeLoaderTest, ColorsThatAreNotATableAreSurvived) {
            const LoadedTheme loaded = parsed(R"(
name   = "odd"
colors = "surprise"
)");

            EXPECT_TRUE(warned_about(loaded, "colors"));
            EXPECT_EQ(loaded.theme.color(app::ThemeColor::Caret), app::Theme{}.color(app::ThemeColor::Caret));
        }

        // --- Finding ------------------------------------------------------------

        TEST(ThemeLoaderTest, AUserThemeOverridesABuiltInOfTheSameName) {
            // How somebody customises a theme: copy it, edit it, and have the
            // original stop winning.
            const testing::TempEnv env;
            const std::filesystem::path user = env.config() / "themes";
            std::filesystem::create_directories(user);
            {
                std::ofstream file{user / "typeit-dark.toml"};
                file << "name = \"typeit-dark\"\n[colors]\ncaret = \"#010203\"\n";
            }

            const core::Result<LoadedTheme> loaded = find_theme("typeit-dark", user, shipped_themes());

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->theme.color(app::ThemeColor::Caret),
                      (app::Rgb{.red = 0x01, .green = 0x02, .blue = 0x03}));
        }

        TEST(ThemeLoaderTest, ABuiltInIsFoundWhenTheUserHasNoneOfThatName) {
            const testing::TempEnv env;

            const core::Result<LoadedTheme> loaded = find_theme("mono", env.config() / "themes", shipped_themes());

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->theme.name, "mono");
        }

        TEST(ThemeLoaderTest, AThemeNobodyHasIsReportedByName) {
            const testing::TempEnv env;

            const core::Result<LoadedTheme> loaded =
                    find_theme("chartreuse", env.config() / "themes", shipped_themes());

            ASSERT_FALSE(loaded);
            EXPECT_EQ(loaded.error().code, core::ErrorCode::UnknownTheme);
            EXPECT_EQ(loaded.error().context, "chartreuse");
        }

        TEST(ThemeLoaderTest, AMissingFileIsReportedRatherThanCrashedOn) {
            const testing::TempEnv env;

            const core::Result<LoadedTheme> loaded = load_theme(env.config() / "nothing.toml");

            ASSERT_FALSE(loaded);
            EXPECT_EQ(loaded.error().code, core::ErrorCode::FileNotFound);
        }

        TEST(ThemeLoaderTest, AFailingFileNamesItself) {
            // A line number in a file nobody said the name of is no use when
            // two themes are being edited.
            const testing::TempEnv env;
            const std::filesystem::path broken = env.config() / "broken.toml";
            {
                std::ofstream file{broken};
                file << "= = =\n";
            }

            const core::Result<LoadedTheme> loaded = load_theme(broken);

            ASSERT_FALSE(loaded);
            EXPECT_NE(loaded.error().context.find("broken.toml"), std::string::npos) << loaded.error().context;
        }

        // --- The value type ------------------------------------------------------

        TEST(ThemeLoaderTest, EveryColourHasAKeyAndEveryKeyIsFound) {
            // The table that maps them is the only thing keeping the loader and
            // any future settings screen agreeing about spelling.
            for (const app::ThemeColor which: app::kAllThemeColors) {
                const std::string_view key = app::to_string(which);
                EXPECT_NE(key, "unknown") << static_cast<int>(which);

                app::ThemeColor found{};
                EXPECT_TRUE(app::theme_color_from(key, found)) << key;
                EXPECT_EQ(found, which) << key;
            }
        }

        TEST(ThemeLoaderTest, HexRoundTrips) {
            for (const std::string_view spelled: {"#000000", "#ffffff", "#1e1e2e", "#89b4fa"}) {
                app::Rgb parsed_color;
                ASSERT_TRUE(app::rgb_from_hex(spelled, parsed_color)) << spelled;
                EXPECT_EQ(app::to_hex(parsed_color), spelled);
            }
        }

        TEST(ThemeLoaderTest, HexIsCaseInsensitiveOnTheWayInAndLowerOnTheWayOut) {
            app::Rgb upper;
            ASSERT_TRUE(app::rgb_from_hex("#AABBCC", upper));
            EXPECT_EQ(app::to_hex(upper), "#aabbcc");
        }

        TEST(ThemeLoaderTest, MalformedHexIsRefused) {
            app::Rgb ignored;
            for (const std::string_view bad: {"", "#", "aabbcc", "#aabbc", "#aabbccd", "#gggggg", "#12 456"}) {
                EXPECT_FALSE(app::rgb_from_hex(bad, ignored)) << "accepted \"" << bad << "\"";
            }
        }

    }  // namespace
}  // namespace typeit::infra
