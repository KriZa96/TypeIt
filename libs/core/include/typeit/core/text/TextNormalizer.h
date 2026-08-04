// Turning a file someone found on the internet into text that can be typed.
//
// A domain rule, not an import detail, which is why it lives in core: the same
// pipeline decides what the user is scored against, what the deduplicating
// hash is taken over, and what a bookmark's offset means. Two of those
// disagreeing is a bug nobody can see until it has already happened.
//
// The steps run in the order documented in GAMEPLAY §5.2 and each one is
// individually toggleable. The order is pinned by test rather than by comment,
// because a comment saying "order matters" has never once stopped anyone from
// reordering.
#ifndef TYPEIT_CORE_TEXT_TEXTNORMALIZER_H
#define TYPEIT_CORE_TEXT_TEXTNORMALIZER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "typeit/core/util/Result.h"

namespace typeit::core {

    struct NormalizeOptions {
        /// CRLF and lone CR become LF, so a file written on Windows and a file
        /// written on Linux are the same text.
        bool line_endings = true;

        /// `e` + U+0301 becomes `é`. Without this the same text saved from two
        /// editors hashes to two different values and imports twice.
        bool nfc = true;

        /// Curly quotes, en and em dashes and ellipses become their ASCII
        /// equivalents. On by default: `"` is on the keyboard and `"` is not,
        /// and a typing test that cannot be passed is not a typing test.
        bool flatten_typography = true;

        /// Runs of spaces and tabs within a line become one space, and
        /// trailing whitespace goes. Line structure is left alone — collapsing
        /// blank lines would run the paragraphs of a novel together.
        bool collapse_whitespace = true;

        /// The "simplify" toggles. Off by default: punctuation and capitals
        /// are most of what makes typing practice worth doing.
        bool strip_punctuation = false;
        bool lowercase = false;

        /// Tabs become `tab_width` spaces. Note that with `collapse_whitespace`
        /// on there are no tabs left by the time this runs — the documented
        /// order puts collapsing first. Expanding tabs is for the case where
        /// collapsing is off, which is what importing code looks like.
        bool expand_tabs = true;
        std::size_t tab_width = 4;
    };

    /// The canonical combining class. Zero for the vast majority of Unicode,
    /// which is exactly what makes the combining-mark path cheap.
    [[nodiscard]] std::uint8_t combining_class(char32_t code_point) noexcept;

    /// Whether Unicode calls this punctuation (Pc, Pd, Ps, Pe, Pi, Pf, Po).
    /// Symbols are not punctuation here: `+` and `=` are what a maths text is
    /// about, and removing them would not simplify it.
    [[nodiscard]] bool is_punctuation(char32_t code_point) noexcept;

    /// The simple lowercase mapping, or the code point unchanged.
    ///
    /// Simple, not full: the mappings that grow — German sharp s to `ss` — are
    /// conditional on language, and a typing test that quietly lengthens the
    /// text has changed what the user agreed to type.
    [[nodiscard]] char32_t to_lowercase(char32_t code_point) noexcept;

    /// Unicode Normalization Form C: decompose canonically, order the
    /// combining marks, compose back.
    ///
    /// Hangul is handled arithmetically rather than from a table, per UAX #15
    /// §16 — eleven thousand syllables of generated data would be standing in
    /// for four lines of code.
    [[nodiscard]] std::u32string to_nfc(std::u32string_view text);

    /// Decodes, normalises, and re-encodes.
    ///
    /// Invalid UTF-8 is an `ErrorCode::InvalidUtf8` naming the byte offset,
    /// because "invalid UTF-8 somewhere in a 40 kB file" is not a diagnosis.
    /// Everything else is total: any valid input has a normalised form, and
    /// that form is the same every time — no clock, no locale, no filesystem.
    [[nodiscard]] Result<std::string> normalize(std::string_view text, const NormalizeOptions& options = {});

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_TEXTNORMALIZER_H
