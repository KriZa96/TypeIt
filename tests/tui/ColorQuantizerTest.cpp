// One theme, every terminal.
//
// The property that matters is the last one: the colours a *typing state*
// depends on must stay distinguishable all the way down to sixteen. A theme
// where "incorrect" and "corrected" collapse to the same red is a theme that
// lies about what happened, and it should fail here rather than on somebody's
// terminal.

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <ftxui/screen/color.hpp>
#include <gtest/gtest.h>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "ColorQuantizer.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/theme/ThemeLoader.h"

namespace typeit::tui {
    namespace {

        app::Rgb rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
            return app::Rgb{.red = red, .green = green, .blue = blue};
        }

        /// The colours a typing state is told apart by. Deliberately not all
        /// fourteen: `background`, `surface` and `border` are three shades of
        /// the same dark, and demanding they stay distinct in a sixteen-colour
        /// palette would force every theme to be garish. What must survive is
        /// what the typist reads meaning from.
        constexpr std::array<app::ThemeColor, 5> kStateColors{
                app::ThemeColor::TextPending,   app::ThemeColor::TextCorrect, app::ThemeColor::TextIncorrect,
                app::ThemeColor::TextCorrected, app::ThemeColor::Caret,
        };

        // --- The 256 cube ------------------------------------------------------

        TEST(ColorQuantizerTest, KnownColoursMapToTheirXtermIndices) {
            // Against the standard table: 16 is the cube's black, 231 its
            // white, 196 pure red, 46 pure green, 21 pure blue.
            EXPECT_EQ(xterm256_index(rgb(0, 0, 0)), 16);
            EXPECT_EQ(xterm256_index(rgb(255, 255, 255)), 231);
            EXPECT_EQ(xterm256_index(rgb(255, 0, 0)), 196);
            EXPECT_EQ(xterm256_index(rgb(0, 255, 0)), 46);
            EXPECT_EQ(xterm256_index(rgb(0, 0, 255)), 21);
        }

        TEST(ColorQuantizerTest, GreysUseTheGreyRampRatherThanTheCube) {
            // The cube's nearest entry to a mid grey is visibly tinted; the
            // 24-step ramp is what the ramp is for.
            for (const std::uint8_t level: {std::uint8_t{0x40}, std::uint8_t{0x80}, std::uint8_t{0xc0}}) {
                const std::uint8_t index = xterm256_index(rgb(level, level, level));
                EXPECT_GE(index, 232) << "grey " << static_cast<int>(level) << " left the ramp";
                EXPECT_LE(index, 255);
            }
        }

        TEST(ColorQuantizerTest, NearBlackAndNearWhiteTakeTheCubesOwnEnds) {
            EXPECT_EQ(xterm256_index(rgb(2, 2, 2)), 16);
            EXPECT_EQ(xterm256_index(rgb(250, 250, 250)), 231);
        }

        TEST(ColorQuantizerTest, TheGreyRampIsOrdered) {
            // A darker grey never maps to a lighter index.
            std::uint8_t previous = 0;
            for (int level = 16; level < 236; level += 10) {
                const std::uint8_t index =
                        xterm256_index(rgb(static_cast<std::uint8_t>(level), static_cast<std::uint8_t>(level),
                                           static_cast<std::uint8_t>(level)));
                EXPECT_GE(index, previous) << "at " << level;
                previous = index;
            }
        }

        // --- The 16 colours ----------------------------------------------------

        TEST(ColorQuantizerTest, KnownColoursMapToTheirAnsiIndices) {
            EXPECT_EQ(ansi16_index(rgb(0, 0, 0)), 0);
            EXPECT_EQ(ansi16_index(rgb(255, 255, 255)), 15);
            EXPECT_EQ(ansi16_index(rgb(128, 128, 128)), 7) << "a mid grey is a grey, not a colour";

            // Hue decides which colour, the brightest channel decides whether
            // it is the bright variant.
            EXPECT_EQ(ansi16_index(rgb(255, 0, 0)), 9);
            EXPECT_EQ(ansi16_index(rgb(170, 0, 0)), 1);
            EXPECT_EQ(ansi16_index(rgb(0, 255, 0)), 10);
            EXPECT_EQ(ansi16_index(rgb(0, 0, 255)), 12);
            EXPECT_EQ(ansi16_index(rgb(255, 255, 0)), 11);
        }

        TEST(ColorQuantizerTest, APastelKeepsItsHueRatherThanCollapsingToWhite) {
            // The defect this replaced: by nearest-RGB distance a pastel pink
            // is closer to white than to red, so a theme of pastels came out as
            // two shades of white and every typing state looked the same.
            EXPECT_EQ(ansi16_index(rgb(0xf3, 0x8b, 0xa8)), 9) << "pink is a red";
            EXPECT_EQ(ansi16_index(rgb(0x89, 0xb4, 0xfa)), 12) << "and pale blue is a blue";
            EXPECT_EQ(ansi16_index(rgb(0xa6, 0xe3, 0xa1)), 10);
            EXPECT_EQ(ansi16_index(rgb(0xf9, 0xe2, 0xaf)), 11);
        }

        TEST(ColorQuantizerTest, TheMappingPreservesPerceptualOrdering) {
            // A darker colour never maps to a lighter one. Checked along the
            // grey axis, which is where a mistake would be worst and where the
            // ordering is unambiguous: text meant to recede must not come out
            // brighter than text meant to stand out.
            //
            // The four greys, in the order a terminal draws them.
            const std::array<std::uint8_t, 4> ordered{0, 8, 7, 15};

            std::size_t reached = 0;
            for (int level = 0; level <= 255; level += 5) {
                const auto shade = static_cast<std::uint8_t>(level);
                const std::uint8_t index = ansi16_index(rgb(shade, shade, shade));

                const auto at =
                        static_cast<std::size_t>(std::distance(ordered.begin(), std::ranges::find(ordered, index)));
                ASSERT_LT(at, ordered.size()) << "grey " << level << " left the grey ramp";
                EXPECT_GE(at, reached) << "grey " << level << " mapped darker than a darker grey";
                reached = at;
            }
        }

        TEST(ColorQuantizerTest, BlackAndWhiteNeverSwap) {
            EXPECT_NE(ansi16_index(rgb(0, 0, 0)), ansi16_index(rgb(255, 255, 255)));
            EXPECT_LT(ansi16_index(rgb(0, 0, 0)), 8);
            EXPECT_GE(ansi16_index(rgb(255, 255, 255)), 8);
        }

        // --- Depths -------------------------------------------------------------

        TEST(ColorQuantizerTest, TruecolorPassesThroughUnchanged) {
            const ftxui::Color exact = quantize(rgb(0x1e, 0x1e, 0x2e), app::ColorDepth::TrueColor);

            EXPECT_EQ(exact, ftxui::Color::RGB(0x1e, 0x1e, 0x2e));
        }

        TEST(ColorQuantizerTest, MonoUsesNoColourAtAll) {
            for (const app::ThemeColor which: app::kAllThemeColors) {
                const Styling styling = style_for(app::Theme{}, which, app::ColorDepth::Mono);
                EXPECT_EQ(styling.color, ftxui::Color::Default) << static_cast<int>(which);
            }
        }

        TEST(ColorQuantizerTest, MonoTellsTheTypingStatesApartWithAttributes) {
            // The four states have to be distinguishable with no colour, and
            // underline, reverse and bold are the only three ways left.
            const app::Theme theme;
            const Styling pending = style_for(theme, app::ThemeColor::TextPending, app::ColorDepth::Mono);
            const Styling correct = style_for(theme, app::ThemeColor::TextCorrect, app::ColorDepth::Mono);
            const Styling incorrect = style_for(theme, app::ThemeColor::TextIncorrect, app::ColorDepth::Mono);
            const Styling corrected = style_for(theme, app::ThemeColor::TextCorrected, app::ColorDepth::Mono);

            const auto shape = [](const Styling& styling) {
                return std::make_tuple(styling.bold, styling.underline, styling.inverted);
            };

            const std::set<std::tuple<bool, bool, bool>> distinct{shape(pending), shape(correct), shape(incorrect),
                                                                  shape(corrected)};
            EXPECT_EQ(distinct.size(), 4U) << "two typing states look identical in mono";
        }

        TEST(ColorQuantizerTest, MonoDrawsEverythingElsePlain) {
            // A terminal that underlined everything would be worse than one
            // that underlined nothing.
            const Styling muted = style_for(app::Theme{}, app::ThemeColor::Muted, app::ColorDepth::Mono);

            EXPECT_FALSE(muted.bold);
            EXPECT_FALSE(muted.underline);
            EXPECT_FALSE(muted.inverted);
        }

        TEST(ColorQuantizerTest, ColourDepthsAboveMonoCarryNoAttributes) {
            for (const app::ColorDepth depth:
                 {app::ColorDepth::Ansi16, app::ColorDepth::Ansi256, app::ColorDepth::TrueColor}) {
                const Styling styling = style_for(app::Theme{}, app::ThemeColor::TextIncorrect, depth);
                EXPECT_FALSE(styling.underline) << app::to_string(depth);
                EXPECT_FALSE(styling.bold) << app::to_string(depth);
            }
        }

        TEST(ColorQuantizerTest, QuantisationIsDeterministicAndIdempotent) {
            // The same colour, twice, at every depth — and quantising an
            // already-quantised colour changes nothing further.
            for (const app::ColorDepth depth: {app::ColorDepth::Mono, app::ColorDepth::Ansi16, app::ColorDepth::Ansi256,
                                               app::ColorDepth::TrueColor}) {
                for (const app::ThemeColor which: app::kAllThemeColors) {
                    const app::Rgb color = app::Theme{}.color(which);
                    EXPECT_EQ(quantize(color, depth), quantize(color, depth));
                }
            }

            // Idempotence where it means something: a colour that is already
            // exactly a palette entry maps to that entry rather than drifting
            // to a neighbour. The cube's levels are 0, 95, 135, 175, 215, 255.
            EXPECT_EQ(xterm256_index(rgb(95, 135, 175)), 16 + (36 * 1) + (6 * 2) + 3);
            EXPECT_EQ(xterm256_index(rgb(255, 0, 0)), 196);
            EXPECT_EQ(xterm256_index(rgb(0, 0, 0)), 16);
        }

        // --- The property that matters --------------------------------------------

        TEST(ColorQuantizerTest, EveryShippedThemeStaysReadableAtEveryDepth) {
            // The property that matters, over the files this project actually
            // ships rather than over a copy of their values: if two typing
            // states collapse to one colour, the theme lies about what
            // happened, and it should fail here rather than on somebody's
            // terminal.
            //
            // `mono` is excluded by name and on purpose — it has no colour at
            // all, which is its whole point, and `style_for` tells its states
            // apart with attributes instead. There is a test for that above.
            for (const std::string& name: infra::built_in_theme_names()) {
                if (name == "mono") {
                    continue;
                }

                const core::Result<infra::LoadedTheme> loaded =
                        infra::load_theme(std::filesystem::path{TYPEIT_ASSETS_DIR} / "themes" / (name + ".toml"));
                ASSERT_TRUE(loaded) << name;

                // Keyed on what the quantiser decided, not on the escape
                // sequence FTXUI prints for it: `Color::Print` is not the thing
                // under test, and CI's FTXUI does not spell it the way this
                // machine's does — which made this test pass locally and fail
                // there, for a reason that had nothing to do with colours.
                std::set<std::uint32_t> at_256;
                std::set<std::uint32_t> at_16;
                std::set<std::uint32_t> exact;

                for (const app::ThemeColor which: kStateColors) {
                    const app::Rgb color = loaded->theme.color(which);
                    const auto packed =
                            static_cast<std::uint32_t>((color.red << 16U) | (color.green << 8U) | color.blue);

                    EXPECT_TRUE(exact.insert(packed).second)
                            << name << ": " << app::to_string(which) << " collapses at truecolor";
                    EXPECT_TRUE(at_256.insert(xterm256_index(color)).second)
                            << name << ": " << app::to_string(which) << " collapses at 256";
                    EXPECT_TRUE(at_16.insert(ansi16_index(color)).second)
                            << name << ": " << app::to_string(which) << " collapses at 16";
                }
            }
        }

        TEST(ColorQuantizerTest, TheMonoThemeIsTheOneAllowedToCollapse) {
            // `mono` maps every colour to the same nothing on purpose — that is
            // what the attributes are for, and asserting otherwise would
            // demand colour from a theme whose whole point is having none.
            const Styling correct = style_for(app::Theme{}, app::ThemeColor::TextCorrect, app::ColorDepth::Mono);
            const Styling incorrect = style_for(app::Theme{}, app::ThemeColor::TextIncorrect, app::ColorDepth::Mono);

            EXPECT_EQ(correct.color, incorrect.color);
            EXPECT_NE(correct.underline, incorrect.underline) << "and the attributes carry it instead";
        }

    }  // namespace
}  // namespace typeit::tui
