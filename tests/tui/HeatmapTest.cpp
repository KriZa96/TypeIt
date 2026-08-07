// The keyboard, coloured by error rate (TI-102).
//
// The assertion that matters most is the one about missing data: a key never
// pressed and a key never missed must not look the same, because the whole
// point of the picture is to say what to practise, and confusing those two
// says the opposite of the truth.

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "Heatmap.h"
#include "Snapshot.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/metrics/KeyStats.h"

namespace typeit::tui {
    namespace {

        core::KeyStats a_few_keys() {
            core::KeyStats stats;
            stats.per_grapheme["a"] = {.attempts = 100, .errors = 0};    // Perfect.
            stats.per_grapheme["s"] = {.attempts = 100, .errors = 3};    // A little off.
            stats.per_grapheme["d"] = {.attempts = 100, .errors = 30};   // Bad.
            stats.per_grapheme[","] = {.attempts = 20, .errors = 8};     // Punctuation counts.
            return stats;
        }

        TEST(HeatmapTest, TheLayoutIsTheOneUnderTheTypistsHands) {
            const std::vector<std::string>& rows = qwerty_rows();

            ASSERT_EQ(rows.size(), 4U);
            EXPECT_EQ(rows.at(1), "qwertyuiop[]");
            EXPECT_EQ(rows.at(2), "asdfghjkl;'");
            EXPECT_EQ(rows.at(3), "zxcvbnm,./");
        }

        TEST(HeatmapTest, PunctuationAndDigitsAreIncludedRatherThanOnlyLetters) {
            // A typist who misses every comma learns nothing from a picture
            // that does not have one, and the stats have the data either way.
            const std::vector<std::string>& rows = qwerty_rows();
            std::string every;
            for (const std::string& row: rows) {
                every += row;
            }

            for (const char expected: {',', '.', ';', '/', '1', '0', '-'}) {
                EXPECT_NE(every.find(expected), std::string::npos) << "missing " << expected;
            }
        }

        TEST(HeatmapTest, TheIntensityRampIsMonotonicInTheErrorRate) {
            // Worse must never draw lighter. Asserted over both glyph sets,
            // because `Mono` has nothing but the glyph to say it with.
            for (const app::GlyphSet set: {app::GlyphSet::Unicode, app::GlyphSet::Ascii}) {
                std::vector<std::string> seen;
                for (const double rate: {0.0, 0.01, 0.03, 0.07, 0.15, 0.4, 1.0}) {
                    std::string glyph{intensity_glyph(rate, set)};
                    if (seen.empty() || seen.back() != glyph) {
                        // Each new glyph must be one nobody has used yet, or
                        // the ramp has doubled back on itself.
                        EXPECT_EQ(std::ranges::find(seen, glyph), seen.end()) << "rate " << rate;
                        seen.push_back(std::move(glyph));
                    }
                }
                EXPECT_GT(seen.size(), 2U) << "the ramp must actually vary";
            }
        }

        TEST(HeatmapTest, NoDataLooksDifferentFromNoErrors) {
            // The assertion this widget exists for. `a` was typed perfectly;
            // `p` was never typed at all. Drawing both as "fine" would send
            // somebody to practise everything except the keys they get wrong.
            app::Theme theme;
            core::KeyStats stats;
            stats.per_grapheme["a"] = {.attempts = 100, .errors = 0};

            const std::string drawn = testing::render_to_text(
                    heatmap(stats, theme, {.glyphs = app::GlyphSet::Ascii}), 40, 6);

            EXPECT_NE(drawn.find("a "), std::string::npos) << "typed and clean:\n" << drawn;
            EXPECT_NE(drawn.find("p?"), std::string::npos) << "never typed, and marked as such:\n" << drawn;
        }

        TEST(HeatmapTest, ItWorksAtEveryColourDepth) {
            app::Theme theme;
            const core::KeyStats stats = a_few_keys();

            for (const app::ColorDepth depth:
                 {app::ColorDepth::TrueColor, app::ColorDepth::Ansi256, app::ColorDepth::Ansi16,
                  app::ColorDepth::Mono}) {
                const std::string drawn =
                        testing::render_to_text(heatmap(stats, theme, {.depth = depth}), 40, 6);

                EXPECT_NE(drawn.find('q'), std::string::npos) << "depth " << static_cast<int>(depth);
                EXPECT_NE(drawn.find('a'), std::string::npos) << "depth " << static_cast<int>(depth);
            }
        }

        TEST(HeatmapTest, InMonoTheIntensityIsCarriedByTheGlyph) {
            // With no colour there has to be something else, or a heatmap is a
            // picture of a keyboard.
            app::Theme theme;
            core::KeyStats stats;
            stats.per_grapheme["a"] = {.attempts = 100, .errors = 0};
            stats.per_grapheme["s"] = {.attempts = 100, .errors = 40};

            const std::string drawn = testing::render_to_text(
                    heatmap(stats, theme, {.glyphs = app::GlyphSet::Ascii, .depth = app::ColorDepth::Mono}), 40, 6);

            EXPECT_NE(drawn.find("a "), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("s#"), std::string::npos) << "the bad key is dense:\n" << drawn;
        }

        TEST(HeatmapTest, AKeyWithNoAttemptsHasNoRateRatherThanARateOfZero) {
            // Zero is the *best possible* rate, so it cannot double as "unknown".
            EXPECT_FALSE(error_rate(core::KeyStat{}).has_value());
            EXPECT_TRUE(error_rate(core::KeyStat{.attempts = 1, .errors = 0}).has_value());
            EXPECT_DOUBLE_EQ(*error_rate(core::KeyStat{.attempts = 4, .errors = 1}), 0.25);
        }

        TEST(HeatmapTest, AnEmptyStatSetRendersTheKeyboardWithNoDataEverywhere) {
            app::Theme theme;

            const std::string drawn =
                    testing::render_to_text(heatmap({}, theme, {.glyphs = app::GlyphSet::Ascii}), 40, 6);

            EXPECT_NE(drawn.find("q?"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("z?"), std::string::npos) << drawn;
        }

        TEST(HeatmapTest, SnapshotForAKnownStatSet) {
            app::Theme theme;
            theme.name = "snapshot";

            testing::expect_matches_golden("heatmap_80x24",
                                           testing::render_to_text(heatmap(a_few_keys(), theme), 40, 5));
        }

    }  // namespace
}  // namespace typeit::tui
