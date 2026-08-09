// Extractor boundaries onto the normalised text (TX-005, TEXT_SOURCES §9).
//
// An extractor knows where a chapter starts in the bytes it produced.
// Normalisation then rewrites those bytes — composing marks, collapsing runs of
// spaces, flattening an em dash into two characters that are not the same
// length — so every one of those offsets is stale by the time anything stores
// it. Handing them out unchanged would mean a bookmark for "Chapter 4" landing
// in Chapter 3 on any text with typography in it, which is every book.
//
// So the boundaries are mapped, and the unit they are mapped into is the
// grapheme, because that is what a bookmark is measured in and what the typist
// counts.
#ifndef TYPEIT_APP_INGEST_SECTIONS_H
#define TYPEIT_APP_INGEST_SECTIONS_H

#include <span>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/ingest/Readiness.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/text/TextBuffer.h"

namespace typeit::app {

    /// The same boundaries, re-expressed against the text the typing-readiness
    /// pass produced (TX-007).
    ///
    /// That pass deletes lines and merges others, so an extractor's boundary —
    /// a byte offset into what *it* emitted — points somewhere else afterwards.
    /// Dropping a single running head would otherwise move every chapter marker
    /// in a book up by a line, which is a bookmark for Chapter 20 landing in
    /// Chapter 19 by the end of it.
    [[nodiscard]] std::vector<SectionBoundary> remapped(std::span<const SectionBoundary> boundaries,
                                                        std::string_view extracted, const ReadinessResult& typeable);

    /// The sections of `normalized`, given where the extractor found them in
    /// `extracted`.
    ///
    /// The mapping goes through line numbers rather than through byte
    /// arithmetic: every boundary either extractor produces sits at the start
    /// of a line, and normalisation is the one thing in this pipeline that
    /// promises to leave line structure alone. A byte count would be wrong the
    /// moment a curly quote became a straight one.
    ///
    /// The result always satisfies three things, whatever it is given:
    ///
    /// - it is never empty — a text nobody found structure in is one section,
    ///   because "no sections" and "one section" are the same text and only one
    ///   of them needs handling everywhere downstream;
    /// - the sections are contiguous and non-overlapping, each `end` being the
    ///   next `start`;
    /// - they cover `[0, normalized.size())` exactly, so an offset anywhere in
    ///   the text falls in exactly one of them.
    ///
    /// Those are guarantees rather than expectations of the input: a boundary
    /// past the end, or out of order, is clamped rather than believed.
    [[nodiscard]] std::vector<TextSection> sections_for(std::span<const SectionBoundary> boundaries,
                                                        std::string_view extracted, const core::TextBuffer& normalized);

    /// Which section `offset` falls in — "Chapter 4 of 31" (TX-006).
    ///
    /// Total, because `sections_for` guarantees the cover: every offset in the
    /// text is in exactly one section, an offset past the end is in the last
    /// one, and a text with no sections at all answers zero rather than
    /// refusing. An empty section contains nothing, so an offset on the seam
    /// belongs to the section that starts there.
    [[nodiscard]] std::size_t section_at(std::span<const TextSection> sections, core::GraphemeIndex offset);

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_SECTIONS_H
