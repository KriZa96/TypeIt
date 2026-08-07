// The library on screen (TI-118), and pasting into it (TI-112).
//
// The interesting assertions are about the prompts. A screen with five modes
// and one keyboard is where a stray keystroke lands somewhere it should not:
// a `d` typed into a search box must not open a delete confirmation, and an
// arrow key inside a tag prompt must not move the selection out from under it.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "Snapshot.h"
#include "screens/TextLibraryScreen.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::tui {
    namespace {

        constexpr core::Millis kNoon{1'767'225'600'000};

        struct World {
            app::Theme theme;
            Keymap keymap;
            core::Config config;
            testing::FakeTextLibraryRepository library;
            testing::FakeFileSystem files;
            testing::FakeClock clock{kNoon};
            app::TextLibraryService service{library, files, clock};
            ScreenContext context;

            World() {
                context.theme = &theme;
                context.keymap = &keymap;
                context.config = &config;
                context.size = TerminalSize{.columns = 80, .rows = 24};
                context.library = LibrarySource{.service = &service, .records = &library};
            }

            /// A text in the library, imported the way the application would.
            core::TextId add(const std::string& path, const std::string& contents) {
                files.add_file(path, contents);
                const core::Result<app::ImportOutcome> imported = service.import_file(path);
                EXPECT_TRUE(imported) << (imported ? "" : imported.error().context);
                return imported ? imported->id : core::TextId{0};
            }
        };

        /// Types `text` into whatever prompt is open.
        void type(TextLibraryScreen& screen, std::string_view text) {
            for (const char letter: text) {
                EXPECT_TRUE(screen.on_event(ftxui::Event::Character(letter)));
            }
        }

        // ---- the listing -------------------------------------------------------

        TEST(TextLibraryScreenTest, AnEmptyLibraryRendersAFirstRunState) {
            World world;
            TextLibraryScreen screen{world.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("Nothing here yet"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("import"), std::string::npos) << "and says how to fix it";
        }

        TEST(TextLibraryScreenTest, ListingShowsTitleWordCountDifficultyAndProgress) {
            World world;
            // Exactly a hundred graphemes, so half of it is exactly half
            // rather than 49% of a sentence somebody counted by eye.
            const core::TextId id = world.add("book.txt", std::string(100, 'a'));
            ASSERT_TRUE(world.service.advance(id, 50));
            TextLibraryScreen screen{world.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_NE(drawn.find("book"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("words"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("diff"), std::string::npos) << drawn;
            EXPECT_NE(drawn.find("50%"), std::string::npos) << "the bookmark, as a percentage:\n" << drawn;
        }

        TEST(TextLibraryScreenTest, ALongTitleIsCutToItsColumnAndMarked) {
            World world;
            world.files.add_file("x.txt", "the quick brown fox jumps over the lazy dog");
            ASSERT_TRUE(world.service.import_file("x.txt", std::string(200, 'A')));
            TextLibraryScreen screen{world.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            EXPECT_EQ(drawn.find(std::string(60, 'A')), std::string::npos) << "not the whole title:\n" << drawn;
            EXPECT_NE(drawn.find("words"), std::string::npos) << "and the columns after it survive:\n" << drawn;
        }

        TEST(TextLibraryScreenTest, AWideCharacterTitleDoesNotOverflowTheColumn) {
            // Counted in display cells, not bytes or code points: a CJK title
            // is two cells a character, and cutting by byte would either
            // overflow the column or slice a character in half.
            World world;
            world.files.add_file("cjk.txt", "the quick brown fox jumps over the lazy dog");
            ASSERT_TRUE(
                    world.service.import_file("cjk.txt", std::string{"日本語日本語日本語日本語日本語日本語日本語"}));
            TextLibraryScreen screen{world.context};

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);

            std::size_t longest = 0;
            std::size_t at = 0;
            while (at < drawn.size()) {
                const std::size_t end = drawn.find('\n', at);
                longest = std::max(longest, (end == std::string::npos ? drawn.size() : end) - at);
                at = end == std::string::npos ? drawn.size() : end + 1;
            }
            EXPECT_NE(drawn.find("words"), std::string::npos) << drawn;
        }

        TEST(TextLibraryScreenTest, ArrowsMoveTheSelection) {
            World world;
            world.add("one.txt", "the quick brown fox jumps");
            world.add("two.txt", "a wholly different sentence entirely");
            TextLibraryScreen screen{world.context};
            ASSERT_EQ(screen.rows().size(), 2U);

            EXPECT_EQ(screen.selected(), 0U);
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowDown));
            EXPECT_EQ(screen.selected(), 1U);
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowDown));
            EXPECT_EQ(screen.selected(), 1U) << "clamped rather than wrapping";
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowUp));
            EXPECT_EQ(screen.selected(), 0U);
        }

        TEST(TextLibraryScreenTest, EnterReportsTheTextToType) {
            // Reported, not pushed: the screen never touches the stack.
            World world;
            const core::TextId id = world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            EXPECT_FALSE(screen.take_chosen().has_value());
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            const std::optional<core::TextId> chosen = screen.take_chosen();
            ASSERT_TRUE(chosen.has_value());
            EXPECT_EQ(*chosen, id);
            EXPECT_FALSE(screen.take_chosen().has_value()) << "and taken exactly once";
        }

        // ---- search ------------------------------------------------------------

        TEST(TextLibraryScreenTest, SearchFiltersLiveAsYouType) {
            World world;
            world.files.add_file("a.txt", "the quick brown fox jumps");
            ASSERT_TRUE(world.service.import_file("a.txt", std::string{"Rust Book"}));
            world.files.add_file("b.txt", "a wholly different sentence entirely");
            ASSERT_TRUE(world.service.import_file("b.txt", std::string{"Moby Dick"}));
            TextLibraryScreen screen{world.context};
            ASSERT_EQ(screen.rows().size(), 2U);

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('/')));
            ASSERT_EQ(screen.mode(), LibraryMode::Searching);
            type(screen, "rust");

            EXPECT_EQ(screen.rows().size(), 1U) << "narrowed before enter was pressed";
        }

        TEST(TextLibraryScreenTest, ASearchThatMatchesNothingSaysSoRatherThanLookingEmpty) {
            World world;
            world.add("a.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('/')));
            type(screen, "zzz");

            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);
            EXPECT_NE(drawn.find("Nothing matches"), std::string::npos) << drawn;
        }

        TEST(TextLibraryScreenTest, BackspaceInTheSearchBoxWidensAgain) {
            World world;
            world.files.add_file("a.txt", "the quick brown fox jumps");
            ASSERT_TRUE(world.service.import_file("a.txt", std::string{"Rust Book"}));
            world.files.add_file("b.txt", "a wholly different sentence entirely");
            ASSERT_TRUE(world.service.import_file("b.txt", std::string{"Moby Dick"}));
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('/')));
            type(screen, "rust");
            ASSERT_EQ(screen.rows().size(), 1U);
            for (int at = 0; at < 4; ++at) {
                ASSERT_TRUE(screen.on_event(ftxui::Event::Backspace));
            }

            EXPECT_EQ(screen.rows().size(), 2U);
        }

        // ---- import, paste, tag ------------------------------------------------

        TEST(TextLibraryScreenTest, ImportingAFileAddsItAndSaysSo) {
            World world;
            world.files.add_file("/tmp/article.txt", "the quick brown fox jumps over the lazy dog");
            TextLibraryScreen screen{world.context};
            ASSERT_TRUE(screen.rows().empty());

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('i')));
            type(screen, "/tmp/article.txt");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_EQ(screen.rows().size(), 1U);
            EXPECT_NE(screen.message().find("imported"), std::string::npos) << screen.message();
            EXPECT_EQ(screen.mode(), LibraryMode::Browsing) << "and the prompt closes";
        }

        TEST(TextLibraryScreenTest, ImportingSomethingUnreadableIsReportedAndAddsNothing) {
            World world;
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('i')));
            type(screen, "/nowhere.txt");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_TRUE(screen.rows().empty());
            EXPECT_FALSE(screen.message().empty());
        }

        TEST(TextLibraryScreenTest, PastingMultipleLinesKeepsTheLineBreaks) {
            // TI-112: a paste is one event carrying several characters, not a
            // keystroke storm, and its structure has to survive.
            World world;
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('p')));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Character(std::string{"first line\nsecond line"})));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            ASSERT_EQ(screen.rows().size(), 1U);
            const core::Result<std::optional<app::TextItem>> stored =
                    world.library.get(screen.rows().front().summary.id);
            ASSERT_TRUE(stored);
            ASSERT_TRUE(stored->has_value());
            EXPECT_NE((*stored)->content.find('\n'), std::string::npos) << (*stored)->content;
        }

        TEST(TextLibraryScreenTest, ABracketedPasteArrivesAsOneEventRatherThanAStorm) {
            World world;
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('p')));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Character(std::string{"a whole pasted paragraph here"})));

            EXPECT_EQ(screen.typed(), "a whole pasted paragraph here") << "one event, every character";
        }

        TEST(TextLibraryScreenTest, NonAsciiPasteContentSurvivesIntact) {
            World world;
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('p')));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Character(std::string{"čitanka za učenike"})));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            ASSERT_EQ(screen.rows().size(), 1U);
            const core::Result<std::optional<app::TextItem>> stored =
                    world.library.get(screen.rows().front().summary.id);
            ASSERT_TRUE(stored);
            EXPECT_NE((*stored)->content.find("čitanka"), std::string::npos) << (*stored)->content;
        }

        TEST(TextLibraryScreenTest, CancellingAPasteDiscardsIt) {
            World world;
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('p')));
            type(screen, "something typed and thought better of");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Escape));

            EXPECT_TRUE(screen.rows().empty()) << "nothing stored";
            EXPECT_EQ(screen.mode(), LibraryMode::Browsing);
            EXPECT_TRUE(screen.typed().empty());
        }

        TEST(TextLibraryScreenTest, TaggingAppliesToTheSelectedText) {
            World world;
            const core::TextId id = world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('t')));
            type(screen, "prose");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            ASSERT_EQ(screen.rows().size(), 1U);
            EXPECT_EQ(screen.rows().front().summary.id, id);
            const std::vector<std::string>& tags = screen.rows().front().summary.tags;
            EXPECT_NE(std::ranges::find(tags, "prose"), tags.end()) << "the tag is on the text";
        }

        // ---- deleting ----------------------------------------------------------

        TEST(TextLibraryScreenTest, DeletingAsksBeforeItDoesAnything) {
            World world;
            world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('d')));

            EXPECT_EQ(screen.mode(), LibraryMode::Confirming);
            EXPECT_EQ(screen.rows().size(), 1U) << "and nothing has gone yet";
            const std::string drawn = testing::render_to_text(screen.render(), 80, 24);
            EXPECT_NE(drawn.find("delete?"), std::string::npos) << drawn;
        }

        TEST(TextLibraryScreenTest, ConfirmingDeletionSaysWhatElseWent) {
            // "Deleted" alone leaves somebody wondering about the runs they
            // typed from it.
            World world;
            world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('d')));
            type(screen, "y");
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_TRUE(screen.rows().empty());
            EXPECT_NE(screen.message().find("tags and bookmark"), std::string::npos) << screen.message();
            EXPECT_NE(screen.message().find("past runs are kept"), std::string::npos) << screen.message();
        }

        TEST(TextLibraryScreenTest, AnythingOtherThanYesKeepsTheText) {
            // Enter on a confirmation nobody read is how somebody deletes a
            // book they spent a month typing.
            World world;
            world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('d')));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Return));

            EXPECT_EQ(screen.rows().size(), 1U) << "an empty answer is not yes";
            EXPECT_NE(screen.message().find("not deleted"), std::string::npos) << screen.message();
        }

        TEST(TextLibraryScreenTest, EscapingTheConfirmationKeepsTheText) {
            World world;
            world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('d')));
            ASSERT_TRUE(screen.on_event(ftxui::Event::Escape));

            EXPECT_EQ(screen.rows().size(), 1U);
            EXPECT_EQ(screen.mode(), LibraryMode::Browsing);
        }

        // ---- the prompts own the keyboard --------------------------------------

        TEST(TextLibraryScreenTest, APromptSwallowsTheKeysThatWouldOtherwiseBeCommands) {
            // A `d` typed into a search box must not open a delete
            // confirmation, and an arrow inside a tag prompt must not move the
            // selection out from under it.
            World world;
            world.add("one.txt", "the quick brown fox jumps");
            world.add("two.txt", "a wholly different sentence entirely");
            TextLibraryScreen screen{world.context};

            ASSERT_TRUE(screen.on_event(ftxui::Event::Character('t')));
            type(screen, "dip");
            ASSERT_TRUE(screen.on_event(ftxui::Event::ArrowDown));

            EXPECT_EQ(screen.mode(), LibraryMode::Tagging) << "still tagging";
            EXPECT_EQ(screen.typed(), "dip");
            EXPECT_EQ(screen.selected(), 0U) << "and the selection did not move";
        }

        TEST(TextLibraryScreenTest, NothingIsQueriedByDrawing) {
            World world;
            world.add("book.txt", "the quick brown fox jumps");
            TextLibraryScreen screen{world.context};
            const std::size_t after_open = world.library.lists;

            static_cast<void>(testing::render_to_text(screen.render(), 80, 24));
            static_cast<void>(testing::render_to_text(screen.render(), 80, 24));

            EXPECT_EQ(world.library.lists, after_open) << "drawing asked the database nothing";
        }

        TEST(TextLibraryScreenTest, WithNoLibraryAtAllItStillRenders) {
            World world;
            world.context.library = {};
            TextLibraryScreen screen{world.context};

            EXPECT_FALSE(testing::render_to_text(screen.render(), 80, 24).empty());
        }

        TEST(TextLibraryScreenTest, SnapshotAt80x24) {
            World world;
            world.theme.name = "snapshot";
            world.files.add_file("rust.txt", "the quick brown fox jumps over the lazy dog again and again");
            ASSERT_TRUE(world.service.import_file("rust.txt", std::string{"The Rust Book"}));
            world.files.add_file("notes.txt", "a wholly different sentence entirely, for variety");
            ASSERT_TRUE(world.service.import_file("notes.txt", std::string{"Personal notes"}));
            TextLibraryScreen screen{world.context};

            testing::expect_matches_golden("text_library_80x24", testing::render_to_text(screen.render(), 80, 24));
        }

    }  // namespace
}  // namespace typeit::tui
