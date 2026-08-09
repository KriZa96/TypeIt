// Making an extracted chapter into something somebody can actually type
// (TX-007, TEXT_SOURCES section 8).
//
// The difference between a feature and a trap. An EPUB chapter contains running
// heads, page numbers, footnote markers, words hyphenated across line breaks and
// typography no keyboard produces — and an application that imported it
// unchanged would mark every attempt at `—` wrong, forever, with no explanation.
//
// The pass runs between extraction and normalisation, because normalisation is
// where the text stops being a document and becomes a string of graphemes to
// type: everything here is a judgement about *documents*, and every one of them
// is easier to make while the line structure is still the author's.
//
// Every step is a heuristic and every step can be turned off. Nothing here is
// certain — a repeated short line is *probably* a running head, a hard-wrapped
// block is *probably* prose — so the pass reports what it did rather than doing
// it quietly, and the unmodified bytes are kept beside the result.
#ifndef TYPEIT_APP_INGEST_READINESS_H
#define TYPEIT_APP_INGEST_READINESS_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/text/TextNormalizer.h"

namespace typeit::app {

    /// Which steps run. All on by default, which is what a book wants; the code
    /// extractor turns the lot off, because every one of them would be wrong
    /// about a source file.
    struct ReadinessOptions {
        /// `exam-\nple` → `example`, `well-\nknown` → `well-known`.
        bool dehyphenate = true;
        /// Repeated short lines and page numbers.
        bool drop_running_heads = true;
        /// `[12]` and the superscript digits.
        bool strip_footnote_markers = true;
        /// A leading block of dotted leaders.
        bool drop_table_of_contents = true;
        /// Hard-wrapped lines back into paragraphs — the wrapping is TypeIt's
        /// job, and doing it twice gives a ragged column half the width.
        bool rejoin_paragraphs = true;
        /// Project Gutenberg's header and footer, which are formulaic.
        bool strip_boilerplate = true;

        /// Every step off. What a source file wants: dehyphenation would join
        /// `foo-\nbar`, paragraph rejoin would put a function on one line, and
        /// `[1]` in code is a subscript rather than a footnote.
        [[nodiscard]] static ReadinessOptions none();
    };

    /// A grapheme a standard keyboard cannot produce, and how often it occurs.
    struct UnreachableGrapheme {
        std::string text;
        std::size_t count = 0;
        /// Whether normalisation will turn it into something typeable. An em
        /// dash will; `č` will not, and that is the difference between "this is
        /// handled" and "you will need a different keyboard layout".
        bool flattened = false;
    };

    /// What the pass did, and what it found it could not fix.
    ///
    /// Counts rather than a boolean apiece: "3 running heads dropped" is a
    /// sentence somebody can check against their own file, and "cleaned up" is
    /// not.
    struct ReadinessReport {
        std::size_t dehyphenated = 0;
        std::size_t running_heads_dropped = 0;
        std::size_t footnote_markers_stripped = 0;
        std::size_t contents_lines_dropped = 0;
        std::size_t lines_rejoined = 0;
        bool boilerplate_removed = false;

        /// Every grapheme outside printable ASCII, with its count, most
        /// frequent first.
        ///
        /// Judged against ASCII because TypeIt does not know the keyboard in
        /// front of the user. That over-reports for anybody typing their own
        /// language — `č` is one key on a Croatian layout — which is why each
        /// entry says whether normalisation will flatten it. The em dash a PDF
        /// left behind and the `č` in somebody's native prose are different
        /// problems, and a single count would hide both.
        std::vector<UnreachableGrapheme> unreachable;

        /// The graphemes normalisation will *not* rescue. The number worth
        /// showing somebody before they agree to type this.
        [[nodiscard]] std::size_t unreachable_after_normalisation() const;

        /// Whether anything at all happened. A report of nothing is worth not
        /// showing.
        [[nodiscard]] bool empty() const;

        /// One line per thing that happened, for a warnings list or a preview.
        [[nodiscard]] std::vector<std::string> lines() const;
    };

    struct ReadinessResult {
        std::string text;
        ReadinessReport report;

        /// For each line of `text`, the line of the input it began at.
        ///
        /// Sections are the reason this exists. An extractor's chapter
        /// boundaries are line numbers into the text it produced, and this pass
        /// deletes lines and merges others — so without the map, dropping one
        /// running head moves every chapter marker in the book up by a line
        /// (TX-005).
        std::vector<std::size_t> source_lines;

        /// Which line of the result the input's line `line` ended up on.
        [[nodiscard]] std::size_t line_from_source(std::size_t line) const;
    };

    /// The pass. Pure: same text and options in, same result out.
    ///
    /// Idempotent by construction rather than by luck — each step removes the
    /// thing it recognises, so a second run finds nothing left to recognise.
    /// There is a test for it across every combination of the toggles, because
    /// "runs twice" is what re-importing a text after changing a setting does.
    [[nodiscard]] ReadinessResult make_typeable(std::string_view text, const ReadinessOptions& options = {},
                                                const core::NormalizeOptions& normalization = {});

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_READINESS_H
