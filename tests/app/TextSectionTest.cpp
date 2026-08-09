// Chapters, in the unit a bookmark is measured in (TX-005).
//
// The rule the whole thing turns on: the sections of a text cover it end to end
// with no gaps and no overlaps. A bookmark is one number, so wherever it lands
// it has to land inside exactly one section — a gap makes "which chapter am I
// in" unanswerable, and an overlap makes it ambiguous, which is worse.
//
// The second rule is that the offsets are computed *after* normalisation. An
// extractor counts bytes it emitted; normalisation then composes marks,
// collapses runs of spaces and turns an em dash into something a keyboard can
// produce, and every one of those changes the count.

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/ingest/Sections.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        /// The sections of `text` treated as already normalised, which is what
        /// most of these cases want: they are about the mapping, not about
        /// normalisation.
        [[nodiscard]] std::vector<TextSection> sections_of(const std::vector<SectionBoundary>& boundaries,
                                                           std::string_view text) {
            const core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(text);
            EXPECT_TRUE(buffer);
            return sections_for(boundaries, text, *buffer);
        }

        /// Contiguous, non-overlapping, covering `total`. Asserted by every case
        /// rather than by one, because these are invariants of the function and
        /// not a property of one input.
        void expect_covers(const std::vector<TextSection>& sections, std::size_t total) {
            ASSERT_FALSE(sections.empty()) << "a text always has at least one section";
            EXPECT_EQ(sections.front().start.value, 0U) << "the first section starts at the start";
            EXPECT_EQ(sections.back().end.value, total) << "the last one ends at the end";
            for (std::size_t at = 0; at < sections.size(); ++at) {
                EXPECT_EQ(sections[at].idx, at) << "the index is the position";
                EXPECT_LE(sections[at].start.value, sections[at].end.value) << "at " << at;
                if (at + 1 < sections.size()) {
                    EXPECT_EQ(sections[at].end.value, sections[at + 1].start.value)
                            << "a gap or an overlap between " << at << " and " << at + 1;
                }
            }
        }

        // ---- the invariants -------------------------------------------------------

        TEST(TextSectionTest, TheSectionsCoverTheTextWithoutGapOrOverlap) {
            const std::string text = "One\nalpha\nTwo\nbeta\nThree\ngamma\n";
            const std::vector<SectionBoundary> boundaries{
                    {.title = "One", .start = 0, .length = 0},
                    {.title = "Two", .start = text.find("Two"), .length = 0},
                    {.title = "Three", .start = text.find("Three"), .length = 0},
            };

            const std::vector<TextSection> sections = sections_of(boundaries, text);

            ASSERT_EQ(sections.size(), 3U);
            expect_covers(sections, text.size());
            EXPECT_EQ(sections[1].start.value, text.find("Two"));
            EXPECT_EQ(sections[1].end.value, text.find("Three"));
        }

        TEST(TextSectionTest, ATextWithNoSectionsHasOneCoveringEverything) {
            // "No sections" and "one section covering everything" describe the
            // same text, and only one of them needs handling by everything
            // downstream.
            const std::string text = "Just some prose, undivided.\n";

            const std::vector<TextSection> sections = sections_of({}, text);

            ASSERT_EQ(sections.size(), 1U);
            expect_covers(sections, text.size());
            EXPECT_FALSE(sections.front().title.has_value());
        }

        TEST(TextSectionTest, AnEmptyTextStillHasItsOneSection) {
            const std::vector<TextSection> sections = sections_of({}, "");

            ASSERT_EQ(sections.size(), 1U);
            EXPECT_EQ(sections.front().start.value, 0U);
            EXPECT_EQ(sections.front().end.value, 0U);
        }

        TEST(TextSectionTest, WhatComesBeforeTheFirstBoundaryIsASectionToo) {
            // A source file's includes come before its first function, and a
            // bookmark landing in them would otherwise belong to no section at
            // all.
            const std::string text = "#include <cstddef>\n\nint main()\n{\n}\n";
            const std::vector<SectionBoundary> boundaries{
                    {.title = "int main()", .start = text.find("int main()"), .length = 0}};

            const std::vector<TextSection> sections = sections_of(boundaries, text);

            ASSERT_EQ(sections.size(), 2U);
            expect_covers(sections, text.size());
            EXPECT_FALSE(sections[0].title.has_value()) << "the preamble is not called anything";
            EXPECT_EQ(sections[1].title, "int main()");
        }

        TEST(TextSectionTest, TitlesAreOptionalAndAnEmptyOneIsAbsent) {
            // Empty and absent are the same thing said twice, and a section
            // displayed as "" is a blank where a chapter name goes.
            const std::string text = "alpha\nbeta\n";
            const std::vector<SectionBoundary> boundaries{{.title = "", .start = 0, .length = 0},
                                                          {.title = "Named", .start = 6, .length = 0}};

            const std::vector<TextSection> sections = sections_of(boundaries, text);

            ASSERT_EQ(sections.size(), 2U);
            EXPECT_FALSE(sections[0].title.has_value());
            ASSERT_TRUE(sections[1].title.has_value());
            EXPECT_EQ(*sections[1].title, "Named");
        }

        // ---- input this cannot trust ---------------------------------------------

        TEST(TextSectionTest, ABoundaryPastTheEndIsClampedRatherThanBelieved) {
            // Which happens when normalisation drops the tail: a document
            // ending in a heading and nothing after it.
            const std::string text = "alpha\n";
            const std::vector<SectionBoundary> boundaries{{.title = "One", .start = 0, .length = 0},
                                                          {.title = "Off the end", .start = 9'000, .length = 0}};

            const std::vector<TextSection> sections = sections_of(boundaries, text);

            ASSERT_EQ(sections.size(), 2U);
            expect_covers(sections, text.size());
            EXPECT_EQ(sections[1].start.value, text.size()) << "an empty section, not one out of range";
        }

        TEST(TextSectionTest, BoundariesOutOfOrderDoNotProduceAnOverlap) {
            // An overlap is a bookmark in two chapters at once, which is worse
            // than a gap: a gap has no answer and an overlap has two.
            const std::string text = "alpha\nbeta\ngamma\n";
            const std::vector<SectionBoundary> boundaries{{.title = "Second", .start = 11, .length = 0},
                                                          {.title = "First", .start = 0, .length = 0}};

            const std::vector<TextSection> sections = sections_of(boundaries, text);

            expect_covers(sections, text.size());
        }

        // ---- the offsets are not stale --------------------------------------------

        TEST(TextSectionTest, OffsetsAreGraphemesAndNotBytes) {
            // Six bytes, three graphemes, one line. A byte offset read as a
            // grapheme one lands two thirds of the way past where it meant to.
            const std::string text = "čšž\nsecond\n";
            const std::vector<SectionBoundary> boundaries{{.title = "One", .start = 0, .length = 0},
                                                          {.title = "Two", .start = text.find("second"), .length = 0}};

            const std::vector<TextSection> sections = sections_of(boundaries, text);

            ASSERT_EQ(sections.size(), 2U);
            EXPECT_EQ(sections[1].start.value, 4U) << "three letters and a newline, not seven bytes";
        }

        TEST(TextSectionTest, NormalisationMovesTheOffsetsAndTheyMoveWithIt) {
            // The case the whole design exists for. Normalisation collapses the
            // run of spaces and turns the em dash into two characters, so every
            // byte offset the extractor produced is wrong afterwards — and by a
            // different amount than the graphemes moved.
            const std::string extracted = "A line — with     spaces\nThe heading\nand its body\n";
            const std::vector<SectionBoundary> boundaries{
                    {.title = "The heading", .start = extracted.find("The heading"), .length = 0}};

            const core::Result<std::string> normalized = core::normalize(extracted);
            ASSERT_TRUE(normalized);
            ASSERT_NE(*normalized, extracted) << "the case is pointless if normalisation changed nothing";
            const core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(*normalized);
            ASSERT_TRUE(buffer);

            const std::vector<TextSection> sections = sections_for(boundaries, extracted, *buffer);

            ASSERT_EQ(sections.size(), 2U);
            expect_covers(sections, buffer->size());
            EXPECT_EQ(buffer->to_string(sections[1].start, sections[1].end), "The heading\nand its body\n")
                    << "the whole normalised text was: " << *normalized;
            EXPECT_NE(sections[1].start.value, boundaries.front().start)
                    << "and it is not the extractor's offset passed through";
        }

        // ---- through an import ----------------------------------------------------

        class SectionImportTest : public ::testing::Test {
        protected:
            testing::FakeTextLibraryRepository library;
            testing::FakeFileSystem files;
            testing::FakeClock clock{core::Millis{1'700'000'000'000}};
            TextLibraryService service{library, files, clock};

            [[nodiscard]] std::vector<TextSection> import(const std::string& path, const std::string& content) {
                files.add_file(path, content);
                const core::Result<ImportOutcome> outcome = service.import_file(path);
                EXPECT_TRUE(outcome) << (outcome ? "" : outcome.error().context);
                if (!outcome) {
                    return {};
                }
                const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
                EXPECT_TRUE(stored && stored->has_value());
                return stored && stored->has_value() ? (*stored)->sections : std::vector<TextSection>{};
            }
        };

        TEST_F(SectionImportTest, AMarkdownDocumentArrivesWithItsHeadingsAsSections) {
            const std::vector<TextSection> sections =
                    import("/doc.md", "# One\n\nalpha — with an em dash\n\n## Two\n\nbeta\n");

            ASSERT_EQ(sections.size(), 2U);
            EXPECT_EQ(sections[0].title, "One");
            EXPECT_EQ(sections[1].title, "Two");
        }

        TEST_F(SectionImportTest, TheSectionsOfAnImportedTextLandOnTheStoredContent) {
            // The end-to-end version of the staleness case: the offsets are
            // read back against the content the typist will actually see.
            files.add_file("/doc.md", "# One\n\nalpha — with an em dash and     spaces\n\n## Two\n\nbeta\n");
            const core::Result<ImportOutcome> outcome = service.import_file("/doc.md");
            ASSERT_TRUE(outcome) << outcome.error().context;

            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored);
            ASSERT_TRUE(stored->has_value());
            const core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8((*stored)->content);
            ASSERT_TRUE(buffer);

            const std::vector<TextSection>& sections = (*stored)->sections;
            ASSERT_EQ(sections.size(), 2U);
            expect_covers(sections, (*stored)->grapheme_count);
            EXPECT_EQ(buffer->to_string(sections[1].start, sections[1].end), "Two\n\nbeta\n")
                    << "the stored content was: " << (*stored)->content;
        }

        TEST_F(SectionImportTest, APlainFileIsOneSectionCoveringAllOfIt) {
            const std::vector<TextSection> sections = import("/notes.txt", "Nothing structural about this at all.\n");

            ASSERT_EQ(sections.size(), 1U);
            EXPECT_FALSE(sections.front().title.has_value());
            EXPECT_EQ(sections.front().start.value, 0U);
        }

        // ---- what the text remembers about getting here (TX-006) ------------------

        TEST_F(SectionImportTest, AnImportRecordsWhatItWasAndWhatReadIt) {
            // For the first time somebody reports that a file imported wrongly:
            // the answer to "which of eight extractors produced this" is
            // otherwise a guess from the filename.
            files.add_file("/main.py", "def one():\n    return 1\n");
            const core::Result<ImportOutcome> outcome = service.import_file("/main.py");
            ASSERT_TRUE(outcome) << outcome.error().context;

            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored);
            ASSERT_TRUE(stored->has_value());
            EXPECT_EQ((*stored)->mime, "text/x-code");
            EXPECT_EQ((*stored)->extractor, "code");
        }

        TEST_F(SectionImportTest, APasteHasNoTypeToRecord) {
            // It was typed, not detected. Naming a MIME type for it would be
            // recording a fact nobody established.
            const core::Result<ImportOutcome> outcome =
                    service.import_text("Some words somebody pasted in.", TextSource::Paste);
            ASSERT_TRUE(outcome) << outcome.error().context;

            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored);
            ASSERT_TRUE(stored->has_value());
            EXPECT_FALSE((*stored)->mime.has_value());
            EXPECT_FALSE((*stored)->extractor.has_value());
            ASSERT_EQ((*stored)->sections.size(), 1U) << "and still exactly one section";
        }

        TEST_F(SectionImportTest, ABookmarkRecordsWhichSectionItLandedIn) {
            files.add_file("/doc.md", "# One\n\nalpha\n\n## Two\n\nbeta\n\n## Three\n\ngamma\n");
            const core::Result<ImportOutcome> outcome = service.import_file("/doc.md");
            ASSERT_TRUE(outcome) << outcome.error().context;
            const core::Result<std::optional<TextItem>> stored = library.get(outcome->id);
            ASSERT_TRUE(stored && stored->has_value());
            ASSERT_EQ((*stored)->sections.size(), 3U);

            // Somewhere inside the middle chapter, wherever normalisation left
            // it — the point is that the service works it out rather than the
            // caller being asked to.
            ASSERT_TRUE(service.bookmark(outcome->id, (*stored)->sections[1].start));

            const core::Result<std::optional<Bookmark>> mark = library.bookmark(outcome->id);
            ASSERT_TRUE(mark);
            ASSERT_TRUE(mark->has_value());
            EXPECT_EQ((*mark)->section_idx, 1U);
        }

        TEST_F(SectionImportTest, AnOffsetPastTheEndBookmarksTheLastSection) {
            // Reachable through `advance`, which clamps to the end of the text.
            files.add_file("/doc.md", "# One\n\nalpha\n\n## Two\n\nbeta\n");
            const core::Result<ImportOutcome> outcome = service.import_file("/doc.md");
            ASSERT_TRUE(outcome) << outcome.error().context;

            ASSERT_TRUE(service.advance(outcome->id, 100'000));

            const core::Result<std::optional<Bookmark>> mark = library.bookmark(outcome->id);
            ASSERT_TRUE(mark);
            ASSERT_TRUE(mark->has_value());
            EXPECT_EQ((*mark)->section_idx, 1U) << "the last one, not one past it";
        }

        TEST_F(SectionImportTest, BookmarkingATextThatIsNotThereSaysSo) {
            const core::Status marked = service.bookmark(core::TextId{404}, core::GraphemeIndex{0});

            ASSERT_FALSE(marked);
            EXPECT_EQ(marked.error().code, core::ErrorCode::FileNotFound);
        }

        TEST_F(SectionImportTest, ASourceFileGetsASectionPerTopLevelDefinition) {
            // And its indentation is preserved, so the offsets are measured
            // against a normalisation that is not the library's.
            const std::vector<TextSection> sections =
                    import("/main.py", "import sys\n\n\ndef one():\n    return 1\n\n\ndef two():\n    return 2\n");

            ASSERT_EQ(sections.size(), 3U) << "the imports, then each definition";
            EXPECT_FALSE(sections[0].title.has_value());
            EXPECT_EQ(sections[1].title, "def one():");
            EXPECT_EQ(sections[2].title, "def two():");
        }

    }  // namespace
}  // namespace typeit::app
