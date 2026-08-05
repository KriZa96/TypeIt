// Both glyph sets, and the one rule that makes the ASCII set worth having.

#include <cstddef>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <string_view>

#include "Glyphs.h"
#include "typeit/app/Capabilities.h"

namespace typeit::tui {
    namespace {

        TEST(GlyphSetTest, BothSetsDefineEveryGlyph) {
            // Table-driven over the enum, so a new glyph without a form fails
            // here rather than rendering as a question mark on somebody's
            // terminal.
            for (const Glyph which: kAllGlyphs) {
                EXPECT_FALSE(glyph(app::GlyphSet::Unicode, which).empty()) << static_cast<int>(which);
                EXPECT_FALSE(glyph(app::GlyphSet::Ascii, which).empty()) << static_cast<int>(which);
                EXPECT_NE(glyph(app::GlyphSet::Ascii, which), "?") << "no ASCII form for " << static_cast<int>(which);
            }
        }

        TEST(GlyphSetTest, TheAsciiSetContainsNoByteAbove0x7F) {
            // The whole point. A "fallback" containing a multi-byte character
            // is not a fallback, and 1.0's never ran at all — the macro
            // guarding it is defined by no build file.
            for (const Glyph which: kAllGlyphs) {
                for (const char byte: glyph(app::GlyphSet::Ascii, which)) {
                    EXPECT_LT(static_cast<unsigned char>(byte), 0x80U)
                            << "glyph " << static_cast<int>(which) << " is not ASCII";
                }
            }

            for (std::size_t level = 0; level < kSparklineLevels; ++level) {
                for (const char byte: sparkline_level(app::GlyphSet::Ascii, level)) {
                    EXPECT_LT(static_cast<unsigned char>(byte), 0x80U) << "sparkline level " << level;
                }
            }
        }

        TEST(GlyphSetTest, TheUnicodeSetIsActuallyUnicode) {
            // The control: if both sets were ASCII the test above would pass
            // and prove nothing.
            bool any_multibyte = false;
            for (const Glyph which: kAllGlyphs) {
                for (const char byte: glyph(app::GlyphSet::Unicode, which)) {
                    any_multibyte = any_multibyte || static_cast<unsigned char>(byte) >= 0x80U;
                }
            }
            EXPECT_TRUE(any_multibyte);
        }

        TEST(GlyphSetTest, TheSparklineRampIsOrderedAndDistinct) {
            // Eight levels that all looked the same would draw a flat line
            // whatever the data did.
            std::set<std::string> unicode;
            std::set<std::string> ascii;
            for (std::size_t level = 0; level < kSparklineLevels; ++level) {
                unicode.insert(std::string{sparkline_level(app::GlyphSet::Unicode, level)});
                ascii.insert(std::string{sparkline_level(app::GlyphSet::Ascii, level)});
            }

            EXPECT_EQ(unicode.size(), kSparklineLevels);
            EXPECT_EQ(ascii.size(), kSparklineLevels);
        }

        TEST(GlyphSetTest, ALevelPastTheTopDrawsTheTallestBar) {
            // Clamped rather than asserted: a sparkline scales a measurement,
            // and an off-by-one at the top should draw a full bar rather than
            // end the program.
            EXPECT_EQ(sparkline_level(app::GlyphSet::Unicode, 99),
                      sparkline_level(app::GlyphSet::Unicode, kSparklineLevels - 1));
        }

        TEST(GlyphSetTest, SelectionFollowsDetectionUnlessTheConfigurationSaysOtherwise) {
            EXPECT_EQ(glyphs_for("auto", app::GlyphSet::Unicode), app::GlyphSet::Unicode);
            EXPECT_EQ(glyphs_for("auto", app::GlyphSet::Ascii), app::GlyphSet::Ascii);

            // Somebody who has told the program their font cannot draw box
            // characters should not be argued with.
            EXPECT_EQ(glyphs_for("ascii", app::GlyphSet::Unicode), app::GlyphSet::Ascii);
            EXPECT_EQ(glyphs_for("unicode", app::GlyphSet::Ascii), app::GlyphSet::Unicode);
        }

        TEST(GlyphSetTest, AnUnrecognisedConfigurationValueFallsBackToDetection) {
            // The config layer validates the spelling; this is what happens if
            // one ever gets past it.
            EXPECT_EQ(glyphs_for("chartreuse", app::GlyphSet::Unicode), app::GlyphSet::Unicode);
        }

        TEST(GlyphSetTest, TheBorderPiecesAreDistinctInUnicodeAndAllowedToRepeatInAscii) {
            // Unicode has a corner for each corner; ASCII has `+` for all four,
            // which is what the UX table says and is fine — a box drawn with
            // four plus signs is still a box.
            EXPECT_NE(glyph(app::GlyphSet::Unicode, Glyph::BorderTopLeft),
                      glyph(app::GlyphSet::Unicode, Glyph::BorderBottomRight));
            EXPECT_EQ(glyph(app::GlyphSet::Ascii, Glyph::BorderTopLeft), "+");
            EXPECT_EQ(glyph(app::GlyphSet::Ascii, Glyph::BorderBottomRight), "+");
        }

    }  // namespace
}  // namespace typeit::tui
