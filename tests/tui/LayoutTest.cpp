// Layout, as a value rather than a terminal somebody has to drag the corner of.
//
// 1.0 hardcodes 75 columns, 10 rows and a 55-column wrap, so it is unusable
// below about eighty columns and ignores resize entirely. Everything here is a
// function of the reported size, which is what makes every case a line in a
// test.

#include <cstddef>
#include <gtest/gtest.h>

#include "Layout.h"

namespace typeit::tui {
    namespace {

        constexpr std::size_t kFitTerminal = 0;
        constexpr std::size_t kAllRows = 0;

        TEST(LayoutTest, TheLayoutAdaptsToEverySizeWorthSupporting) {
            // UX §6.1's sizes. The assertion is that the text gets wider as the
            // terminal does, which is exactly what a hardcoded 75 cannot do.
            std::size_t previous = 0;
            for (const TerminalSize size:
                 {TerminalSize{.columns = 80, .rows = 24}, TerminalSize{.columns = 100, .rows = 30},
                  TerminalSize{.columns = 120, .rows = 40}, TerminalSize{.columns = 200, .rows = 50}}) {
                const Layout layout = layout_for(size, kFitTerminal, 3, Density::Comfortable);

                EXPECT_FALSE(layout.too_small) << size.columns;
                EXPECT_GT(layout.text_columns, previous) << "at " << size.columns;
                EXPECT_LE(layout.text_columns, size.columns);
                previous = layout.text_columns;
            }
        }

        TEST(LayoutTest, AWidthOfZeroFitsTheTerminal) {
            const Layout layout = layout_for({.columns = 100, .rows = 30}, kFitTerminal, 3, Density::Comfortable);

            EXPECT_GT(layout.text_columns, 80U) << "it used the width it was given";
            EXPECT_LE(layout.text_columns, 100U);
        }

        TEST(LayoutTest, AFixedWidthIsHonouredAndCentred) {
            const Layout layout = layout_for({.columns = 120, .rows = 40}, 60, 3, Density::Comfortable);

            EXPECT_EQ(layout.text_columns, 60U);
            EXPECT_EQ(layout.text_left_margin, 30U) << "centred, not left over";
        }

        TEST(LayoutTest, AFixedWidthWiderThanTheTerminalIsClamped) {
            // A 200-column setting on an 80-column terminal is a request nobody
            // can grant, and drawing off the edge is not the way to answer it.
            const Layout layout = layout_for({.columns = 80, .rows = 24}, 200, 3, Density::Comfortable);

            EXPECT_LE(layout.text_columns, 80U);
        }

        TEST(LayoutTest, TheTwoDensitiesDiffer) {
            const TerminalSize size{.columns = 100, .rows = 30};

            const Layout comfortable = layout_for(size, kFitTerminal, kAllRows, Density::Comfortable);
            const Layout compact = layout_for(size, kFitTerminal, kAllRows, Density::Compact);

            EXPECT_GT(compact.text_columns, comfortable.text_columns) << "compact spends less on margins";
            EXPECT_GT(compact.lines_visible, comfortable.lines_visible) << "and less on chrome";
        }

        TEST(LayoutTest, TheDensityIsReadFromItsConfiguredName) {
            EXPECT_EQ(density_from("compact"), Density::Compact);
            EXPECT_EQ(density_from("comfortable"), Density::Comfortable);
            EXPECT_EQ(density_from("nonsense"), Density::Comfortable) << "the roomier default";
        }

        TEST(LayoutTest, ATerminalTooSmallToUseSaysSo) {
            for (const TerminalSize size:
                 {TerminalSize{.columns = 20, .rows = 24}, TerminalSize{.columns = 80, .rows = 5}}) {
                const Layout layout = layout_for(size, kFitTerminal, 3, Density::Comfortable);

                EXPECT_TRUE(layout.too_small) << size.columns << "x" << size.rows;
                EXPECT_GE(layout.text_columns, 1U) << "and still describes something drawable";
                EXPECT_GE(layout.lines_visible, 1U);
            }
        }

        TEST(LayoutTest, TheSmallestUsableTerminalIsUsable) {
            const Layout layout = layout_for(kMinimumSize, kFitTerminal, 3, Density::Comfortable);

            EXPECT_FALSE(layout.too_small);
            EXPECT_GE(layout.text_columns, 1U);
        }

        TEST(LayoutTest, TheHintBarIsTheFirstThingToGoOnAShortTerminal) {
            // Somebody on a short terminal needs the text and the numbers more
            // than a reminder of which key quits.
            const Layout tall = layout_for({.columns = 80, .rows = 24}, kFitTerminal, 3, Density::Comfortable);
            const Layout short_terminal =
                    layout_for({.columns = 80, .rows = 10}, kFitTerminal, 3, Density::Comfortable);

            EXPECT_TRUE(tall.show_hint_bar);
            EXPECT_TRUE(tall.show_stats_bar);
            EXPECT_FALSE(short_terminal.show_hint_bar);
            EXPECT_TRUE(short_terminal.show_stats_bar);
        }

        TEST(LayoutTest, LinesNeverExceedWhatTheRowsAllow) {
            // A layout that assumed room for the bars and then drew the text
            // off the bottom would be worse than one that showed fewer lines.
            for (std::size_t rows = 10; rows <= 50; ++rows) {
                const Layout layout =
                        layout_for({.columns = 80, .rows = rows}, kFitTerminal, kAllRows, Density::Comfortable);
                EXPECT_LT(layout.lines_visible, rows) << "at " << rows << " rows";
                EXPECT_GE(layout.lines_visible, 1U) << "at " << rows << " rows";
            }
        }

        TEST(LayoutTest, ARapidSequenceOfSizesIsJustASequenceOfAnswers) {
            // The function holds nothing, so a resize cannot leave a stale
            // value behind — which is the whole reason it is a function.
            const Layout first = layout_for({.columns = 100, .rows = 30}, kFitTerminal, 3, Density::Comfortable);

            for (std::size_t columns = 40; columns <= 200; columns += 3) {
                const Layout ignored =
                        layout_for({.columns = columns, .rows = 30}, kFitTerminal, 3, Density::Comfortable);
                EXPECT_GE(ignored.text_columns, 1U);
            }

            const Layout again = layout_for({.columns = 100, .rows = 30}, kFitTerminal, 3, Density::Comfortable);
            EXPECT_EQ(again.text_columns, first.text_columns);
            EXPECT_EQ(again.text_left_margin, first.text_left_margin);
        }

    }  // namespace
}  // namespace typeit::tui
