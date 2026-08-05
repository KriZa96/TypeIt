// The centrepiece, and the two tests that justify the whole rewrite.
//
// `RenderTwiceChangesNothing` is the standing guard against 1.0's
// render-side-effect pattern, where drawing the screen advanced the thing
// being measured. `AMultiByteCharacterIsOneGrapheme` is the direct regression
// test for the README's ćčšđž note, which blames FTXUI for a bug that is
// actually `input_text_.back()` reading one byte of a multi-byte character.

#include <cstddef>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Snapshot.h"
#include "TypingArea.h"
#include "typeit/app/Theme.h"
#include "typeit/core/modes/QuoteMode.h"
#include "typeit/core/session/Session.h"
#include "typeit/core/session/TypingModel.h"
#include "typeit/core/text_supply/WholeTextProvider.h"
#include "typeit/testing/FakeClock.h"

namespace typeit::tui {
    namespace {

        /// A session over `text`, and the area drawn from it. Held together
        /// because the area borrows the session and both have to outlive the
        /// test's assertions.
        struct Area {
            /// Owned, not borrowed. `TypingArea` keeps a pointer to the theme
            /// it was given, so a default argument of `const Theme& = Theme{}`
            /// would bind a temporary that dies when the helper returns —
            /// which ASan caught and a plain build did not.
            app::Theme theme;
            std::unique_ptr<core::Session> session;
            std::shared_ptr<TypingArea> area;
            testing::FakeClock clock{core::Millis{0}};

            void type(std::string_view text) {
                clock.advance(core::Millis{100});
                EXPECT_TRUE(area->OnEvent(ftxui::Event::Character(std::string{text})));
            }

            void backspace() {
                clock.advance(core::Millis{100});
                area->OnEvent(ftxui::Event::Backspace);
            }

            [[nodiscard]] const core::TypingModel& model() const { return session->model(); }

            /// The states as one character each, the same spelling
            /// `testing::TypingRun` uses: `.` pending, `C` correct, `x`
            /// incorrect, `c` corrected, `M` missed.
            [[nodiscard]] std::string states() const {
                std::string rendered;
                for (const core::GraphemeState state: model().states()) {
                    switch (state) {
                        case core::GraphemeState::Pending:
                            rendered += '.';
                            break;
                        case core::GraphemeState::Correct:
                            rendered += 'C';
                            break;
                        case core::GraphemeState::Incorrect:
                            rendered += 'x';
                            break;
                        case core::GraphemeState::Corrected:
                            rendered += 'c';
                            break;
                        case core::GraphemeState::Missed:
                            rendered += 'M';
                            break;
                    }
                }
                return rendered;
            }
        };

        /// Built rather than constructed inline because the area holds a
        /// pointer to the session, so the session has to be moved into place
        /// first.
        std::unique_ptr<Area> an_area(std::string_view text, TypingAreaOptions options = {},
                                      app::Theme theme = app::Theme{}) {
            auto built = std::make_unique<Area>();
            built->theme = std::move(theme);

            core::Result<std::unique_ptr<core::Session>> session = core::Session::create(
                    core::Session::Parts{.mode = std::make_unique<core::QuoteMode>(),
                                         .provider = std::make_unique<core::WholeTextProvider>(text),
                                         .rules = {}},
                    core::Millis{0});
            EXPECT_TRUE(session);
            built->session = session ? std::move(*session) : nullptr;
            built->area = std::make_shared<TypingArea>(*built->session, built->theme, built->clock, options);
            return built;
        }

        /// The theme every snapshot uses. A fixed one, because a snapshot of a
        /// default that somebody later retunes is a snapshot that fails for a
        /// reason nothing to do with rendering.
        app::Theme snapshot_theme() {
            app::Theme theme;
            theme.name = "snapshot";
            return theme;
        }

        // --- The two that matter --------------------------------------------

        TEST(TypingAreaTest, RenderTwiceChangesNothing) {
            // The single most important test in this phase. 1.0's rendering
            // advances the model while drawing it, so a redraw changes what is
            // being measured; this asserts that drawing is a pure function of
            // state, twice over — the same pixels and the same model.
            const std::unique_ptr<Area> area = an_area("hello world");
            area->type("h");
            area->type("e");

            const std::string first = testing::render_to_text(area->area, 40, 5);
            const std::string states_before = area->states();
            const std::size_t cursor_before = area->model().cursor().value;
            const std::size_t events_before = area->model().log().size();

            const std::string second = testing::render_to_text(area->area, 40, 5);

            EXPECT_EQ(first, second);
            testing::expect_render_is_pure(area->area, 40, 5);
            EXPECT_EQ(area->states(), states_before);
            EXPECT_EQ(area->model().cursor().value, cursor_before);
            EXPECT_EQ(area->model().log().size(), events_before) << "rendering logged a keystroke";
        }

        TEST(TypingAreaTest, AMultiByteCharacterIsOneGrapheme) {
            // The README blames FTXUI for this. It is not FTXUI: 1.0 binds an
            // `ftxui::Input` to a string and infers the character typed from
            // `input_text_.back()`, which is the last *byte*. `Event::Character`
            // has always carried the whole sequence.
            const std::unique_ptr<Area> area = an_area("čšž");

            area->type("č");

            EXPECT_EQ(area->model().cursor().value, 1U) << "one keystroke, one grapheme";
            EXPECT_EQ(area->states(), "C..");
            EXPECT_EQ(area->model().log().size(), 1U);
        }

        TEST(TypingAreaTest, EveryMultiByteCharacterInTheReadmesListWorks) {
            const std::unique_ptr<Area> area = an_area("ćčšđž");

            for (const std::string_view letter: {"ć", "č", "š", "đ", "ž"}) {
                area->type(letter);
            }

            EXPECT_EQ(area->states(), "CCCCC");
            EXPECT_EQ(area->model().log().size(), 5U);
        }

        // --- Events ----------------------------------------------------------

        TEST(TypingAreaTest, ACharacterReachesTheModel) {
            const std::unique_ptr<Area> area = an_area("ab");

            area->type("a");

            EXPECT_EQ(area->states(), "C.");
        }

        TEST(TypingAreaTest, AWrongCharacterIsRecordedAsWrong) {
            const std::unique_ptr<Area> area = an_area("ab");

            area->type("x");

            EXPECT_EQ(area->states(), "x.");
        }

        TEST(TypingAreaTest, BackspaceAtPositionZeroIsANoOpAndDoesNotThrow) {
            const std::unique_ptr<Area> area = an_area("ab");

            EXPECT_NO_THROW(area->backspace());

            EXPECT_EQ(area->model().cursor().value, 0U);
            EXPECT_TRUE(area->model().log().empty());
        }

        TEST(TypingAreaTest, APasteIsTypedRatherThanDropped) {
            // A paste arrives as one event with several graphemes in it. Losing
            // all but the first would leave the model disagreeing with what the
            // screen shows, which is worse than either accepting or refusing it.
            const std::unique_ptr<Area> area = an_area("hello");

            area->type("hell");

            EXPECT_EQ(area->model().cursor().value, 4U);
            EXPECT_EQ(area->states(), "CCCC.");
            EXPECT_EQ(area->model().log().size(), 4U) << "one event, four keystrokes";
        }

        TEST(TypingAreaTest, AnEventThatIsNotTextIsLeftForTheScreenAbove) {
            const std::unique_ptr<Area> area = an_area("ab");

            EXPECT_FALSE(area->area->OnEvent(ftxui::Event::Escape));
            EXPECT_FALSE(area->area->OnEvent(ftxui::Event::ArrowLeft));
            EXPECT_TRUE(area->model().log().empty());
        }

        TEST(TypingAreaTest, TheAreaTakesFocusBecauseItTakesKeys) {
            const std::unique_ptr<Area> area = an_area("ab");

            EXPECT_TRUE(area->area->Focusable());
        }

        // --- Rendering ---------------------------------------------------------

        TEST(TypingAreaTest, TheFourStatesRenderDistinctly) {
            // Not a snapshot: what matters is that they differ, and a golden
            // would tie that to one theme's exact colours.
            const std::unique_ptr<Area> pending = an_area("abcd");
            const std::string untouched = testing::render_to_styled_text(pending->area, 20, 3);

            const std::unique_ptr<Area> typed = an_area("abcd");
            typed->type("a");
            const std::string correct = testing::render_to_styled_text(typed->area, 20, 3);

            const std::unique_ptr<Area> wrong = an_area("abcd");
            wrong->type("x");
            const std::string incorrect = testing::render_to_styled_text(wrong->area, 20, 3);

            EXPECT_NE(untouched, correct);
            EXPECT_NE(correct, incorrect);
        }

        TEST(TypingAreaTest, AnIncorrectSpaceRendersAsAnUnderscore) {
            // Nothing else shows a mistake on a character with no ink of its
            // own. 1.0 got this one right and it is worth keeping.
            const std::unique_ptr<Area> area = an_area("a b");
            area->type("a");
            area->type("x");  // Where the space is.

            const std::string drawn = testing::render_to_text(area->area, 20, 3);

            EXPECT_NE(drawn.find('_'), std::string::npos) << drawn;
        }

        TEST(TypingAreaTest, BlindModeHidesTheStateColouring) {
            // Nothing about what was typed changes — only whether the typist
            // can see it while typing.
            const std::unique_ptr<Area> seeing = an_area("abcd", TypingAreaOptions{});
            const std::unique_ptr<Area> blind = an_area("abcd", TypingAreaOptions{.blind = true});
            seeing->type("x");
            blind->type("x");

            EXPECT_NE(testing::render_to_styled_text(seeing->area, 20, 3),
                      testing::render_to_styled_text(blind->area, 20, 3));
            EXPECT_EQ(seeing->states(), blind->states()) << "and the model says the same thing either way";
        }

        // --- Wrapping and resize -------------------------------------------------

        TEST(TypingAreaTest, ResizePreservesCursorAndModelExactly) {
            // The assertion is on the model, not on the render: a resize that
            // redrew correctly but lost a keystroke would pass a snapshot test
            // and fail a typist.
            const std::unique_ptr<Area> area =
                    an_area("the quick brown fox jumps over the lazy dog", TypingAreaOptions{.columns = 80});
            area->type("the quick ");

            const std::string states_before = area->states();
            const std::size_t cursor_before = area->model().cursor().value;
            const std::size_t events_before = area->model().log().size();

            for (const std::size_t columns: {std::size_t{60}, std::size_t{120}, std::size_t{80}}) {
                EXPECT_NE(area->area->Render(), nullptr);  // The model is what is under test.
                TypingAreaOptions resized;
                resized.columns = columns;
                const std::shared_ptr<TypingArea> at_width =
                        std::make_shared<TypingArea>(*area->session, area->theme, area->clock, resized);
                const std::string ignored = testing::render_to_text(at_width, columns, 6);
                EXPECT_FALSE(ignored.empty());

                EXPECT_EQ(area->states(), states_before) << "at " << columns;
                EXPECT_EQ(area->model().cursor().value, cursor_before) << "at " << columns;
                EXPECT_EQ(area->model().log().size(), events_before) << "at " << columns;
            }
        }

        TEST(TypingAreaTest, ARapidSequenceOfResizesDoesNotCorruptState) {
            const std::unique_ptr<Area> area = an_area("the quick brown fox jumps over the lazy dog");
            area->type("the ");
            const std::string states_before = area->states();

            for (std::size_t columns = 20; columns <= 200; columns += 7) {
                TypingAreaOptions resized;
                resized.columns = columns;
                const std::shared_ptr<TypingArea> at_width =
                        std::make_shared<TypingArea>(*area->session, area->theme, area->clock, resized);
                const std::string ignored = testing::render_to_text(at_width, columns, 6);
                EXPECT_FALSE(ignored.empty());
            }

            EXPECT_EQ(area->states(), states_before);
        }

        TEST(TypingAreaTest, TheWidthIsRememberedSoARewrapCanBeSeenToHappen) {
            const std::unique_ptr<Area> area = an_area("the quick brown fox", TypingAreaOptions{.columns = 40});

            EXPECT_FALSE(testing::render_to_text(area->area, 40, 5).empty());

            EXPECT_EQ(area->area->columns(), 40U);
        }

        // --- Snapshots -------------------------------------------------------------

        TEST(TypingAreaTest, SnapshotAt80x24) {
            const std::unique_ptr<Area> area =
                    an_area("the quick brown fox jumps over the lazy dog",
                            TypingAreaOptions{.columns = 78, .lines_visible = 3}, snapshot_theme());
            area->type("the quick ");

            testing::expect_matches_golden("typing_area_80x24", testing::render_to_text(area->area, 80, 24));
        }

        TEST(TypingAreaTest, SnapshotAt120x40) {
            const std::unique_ptr<Area> area =
                    an_area("the quick brown fox jumps over the lazy dog",
                            TypingAreaOptions{.columns = 118, .lines_visible = 3}, snapshot_theme());
            area->type("the quick ");

            testing::expect_matches_golden("typing_area_120x40", testing::render_to_text(area->area, 120, 40));
        }

        TEST(TypingAreaTest, SnapshotAt60x20) {
            const std::unique_ptr<Area> area =
                    an_area("the quick brown fox jumps over the lazy dog",
                            TypingAreaOptions{.columns = 58, .lines_visible = 3}, snapshot_theme());
            area->type("the quick ");

            testing::expect_matches_golden("typing_area_60x20", testing::render_to_text(area->area, 60, 20));
        }

    }  // namespace
}  // namespace typeit::tui
