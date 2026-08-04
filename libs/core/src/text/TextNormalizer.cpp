#include "typeit/core/text/TextNormalizer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/text/NormalizationTables.h"
#include "typeit/core/text/Utf8.h"
#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        /// Unchecked on purpose: every index below sits inside the loop that
        /// bounds it. One accessor with one suppression rather than a
        /// suppression on every line of a tight loop — the same argument as
        /// Utf8.cpp's byte_at.
        constexpr char32_t at(std::u32string_view text, std::size_t index) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[index];
        }

        constexpr char32_t& mutable_at(std::u32string& text, std::size_t index) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[index];
        }

        using tables::CaseMapping;
        using tables::CodeRange;
        using tables::Composition;
        using tables::Decomposition;

        // ---- Hangul (UAX #15 §16) --------------------------------------------
        //
        // Arithmetic, not a table. The eleven thousand precomposed syllables
        // are laid out so that a syllable's leading, vowel and trailing jamo
        // fall out of a division.

        constexpr char32_t kSyllableBase = 0xAC00;
        constexpr char32_t kLeadingBase = 0x1100;
        constexpr char32_t kVowelBase = 0x1161;
        constexpr char32_t kTrailingBase = 0x11A7;
        constexpr char32_t kVowelCount = 21;
        constexpr char32_t kTrailingCount = 28;
        constexpr char32_t kSyllableStride = kVowelCount * kTrailingCount;  // 588
        constexpr char32_t kSyllableCount = 11172;

        constexpr bool is_syllable(char32_t code_point) {
            return code_point >= kSyllableBase && code_point < kSyllableBase + kSyllableCount;
        }

        void decompose_syllable(char32_t code_point, std::u32string& out) {
            const char32_t index = code_point - kSyllableBase;
            out.push_back(kLeadingBase + (index / kSyllableStride));
            out.push_back(kVowelBase + ((index % kSyllableStride) / kTrailingCount));
            if (const char32_t trailing = index % kTrailingCount; trailing != 0) {
                out.push_back(kTrailingBase + trailing);
            }
        }

        /// Zero when the pair is not a syllable, which is the answer for every
        /// code point outside the jamo blocks.
        constexpr char32_t compose_syllable(char32_t first, char32_t second) {
            constexpr char32_t kLeadingCount = 19;
            if (first >= kLeadingBase && first < kLeadingBase + kLeadingCount && second >= kVowelBase &&
                second < kVowelBase + kVowelCount) {
                const char32_t leading = first - kLeadingBase;
                const char32_t vowel = second - kVowelBase;
                return kSyllableBase + (((leading * kVowelCount) + vowel) * kTrailingCount);
            }
            if (is_syllable(first) && (first - kSyllableBase) % kTrailingCount == 0 && second > kTrailingBase &&
                second < kTrailingBase + kTrailingCount) {
                return first + (second - kTrailingBase);
            }
            return 0;
        }

        // ---- table lookups ---------------------------------------------------

        const Decomposition* find_decomposition(char32_t code_point) {
            // libstdc++ makes a std::array iterator a pointer and MSVC makes it a
            // class; spelling it as a pointer compiles on exactly one of the two.
            // NOLINTNEXTLINE(readability-qualified-auto)
            const auto found =
                    std::ranges::lower_bound(tables::kCanonicalDecompositions, code_point, {}, &Decomposition::code);
            if (found == tables::kCanonicalDecompositions.end() || found->code != code_point) {
                return nullptr;
            }
            return &*found;
        }

        char32_t compose_pair(char32_t first, char32_t second) {
            if (const char32_t syllable = compose_syllable(first, second); syllable != 0) {
                return syllable;
            }
            const auto key = std::pair{first, second};
            // libstdc++ makes a std::array iterator a pointer and MSVC makes it a
            // class; spelling it as a pointer compiles on exactly one of the two.
            // NOLINTNEXTLINE(readability-qualified-auto)
            const auto found = std::ranges::lower_bound(
                    tables::kCanonicalCompositions, key, {},
                    [](const Composition& entry) { return std::pair{entry.first, entry.second}; });
            if (found == tables::kCanonicalCompositions.end() || found->first != first || found->second != second) {
                return 0;
            }
            return found->composite;
        }

        bool in_ranges(std::span<const CodeRange> ranges, char32_t code_point) {
            const auto found = std::ranges::upper_bound(ranges, code_point, {}, &CodeRange::first);
            return found != ranges.begin() && code_point <= std::prev(found)->last;
        }

        // ---- the three NFC steps ---------------------------------------------

        void decompose(char32_t code_point, std::u32string& out) {
            if (is_syllable(code_point)) {
                decompose_syllable(code_point, out);
                return;
            }
            const Decomposition* mapping = find_decomposition(code_point);
            if (mapping == nullptr) {
                out.push_back(code_point);
                return;
            }
            // Recursive because the table holds one-step mappings. Bounded at
            // three or so: Unicode does not stack canonical decompositions
            // deeper than that.
            decompose(mapping->first, out);
            if (mapping->second != 0) {
                decompose(mapping->second, out);
            }
        }

        /// Insertion sort by combining class, which is what canonical ordering
        /// is: stable, and only ever moving marks past marks.
        void order_canonically(std::u32string& text) {
            for (std::size_t i = 1; i < text.size(); ++i) {
                const std::uint8_t klass = combining_class(at(text, i));
                if (klass == 0) {
                    continue;
                }
                std::size_t j = i;
                while (j > 0 && combining_class(at(text, j - 1)) > klass) {
                    std::swap(mutable_at(text, j), mutable_at(text, j - 1));
                    --j;
                }
            }
        }

        void compose(std::u32string& text) {
            constexpr std::size_t kNoStarter = std::u32string::npos;
            std::u32string out;
            out.reserve(text.size());

            std::size_t starter = kNoStarter;
            int last_class = -1;  ///< -1 means nothing has followed the starter yet.

            for (const char32_t code_point: text) {
                const int klass = combining_class(code_point);

                // Blocked means something between the starter and here has a
                // class at least as high — composing across it would change
                // which character the mark belongs to.
                if (starter != kNoStarter && last_class < klass) {
                    if (const char32_t composite = compose_pair(at(out, starter), code_point); composite != 0) {
                        mutable_at(out, starter) = composite;
                        continue;
                    }
                }

                out.push_back(code_point);
                if (klass == 0) {
                    starter = out.size() - 1;
                    last_class = -1;
                } else {
                    last_class = klass;
                }
            }

            text = std::move(out);
        }

        // ---- the pipeline steps ----------------------------------------------

        void normalize_line_endings(std::u32string& text) {
            std::u32string out;
            out.reserve(text.size());
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (at(text, i) != U'\r') {
                    out.push_back(at(text, i));
                    continue;
                }
                // A lone CR is a line ending too: it is what a classic Mac
                // file, and a surprising number of exports, still use.
                if (i + 1 < text.size() && at(text, i + 1) == U'\n') {
                    ++i;
                }
                out.push_back(U'\n');
            }
            text = std::move(out);
        }

        /// What flattening replaces, and with what. Anything mapping to more
        /// than one character (the ellipsis) is why this is a string.
        std::u32string_view typographic_replacement(char32_t code_point) {
            switch (code_point) {
                case 0x2018:  // ' ' ' — single quotation marks
                case 0x2019:
                case 0x201A:
                case 0x201B:
                case 0x2032:  // prime, which people paste as an apostrophe
                    return U"'";
                case 0x201C:  // " " " — double quotation marks
                case 0x201D:
                case 0x201E:
                case 0x201F:
                case 0x2033:
                    return U"\"";
                case 0x2010:  // hyphen, figure dash, en dash, em dash, horizontal bar
                case 0x2011:
                case 0x2012:
                case 0x2013:
                case 0x2014:
                case 0x2015:
                    return U"-";
                case 0x2026:  // horizontal ellipsis
                    return U"...";
                case 0x00A0:  // the spaces that are not the space key
                case 0x2007:
                case 0x2009:
                case 0x202F:
                    return U" ";
                default:
                    return {};
            }
        }

        void flatten_typography(std::u32string& text) {
            std::u32string out;
            out.reserve(text.size());
            for (const char32_t code_point: text) {
                const std::u32string_view replacement = typographic_replacement(code_point);
                if (replacement.empty()) {
                    out.push_back(code_point);
                } else {
                    out.append(replacement);
                }
            }
            text = std::move(out);
        }

        constexpr bool is_horizontal_space(char32_t code_point) { return code_point == U' ' || code_point == U'\t'; }

        void collapse_whitespace(std::u32string& text) {
            std::u32string out;
            out.reserve(text.size());
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (!is_horizontal_space(at(text, i))) {
                    out.push_back(at(text, i));
                    continue;
                }
                while (i + 1 < text.size() && is_horizontal_space(at(text, i + 1))) {
                    ++i;
                }
                // Trailing whitespace is whitespace that runs into a newline or
                // into the end of the text; it leaves nothing behind.
                if (i + 1 < text.size() && at(text, i + 1) != U'\n') {
                    out.push_back(U' ');
                }
            }
            text = std::move(out);
        }

        void strip_punctuation(std::u32string& text) {
            std::erase_if(text, [](char32_t code_point) { return is_punctuation(code_point); });
        }

        void lowercase(std::u32string& text) { std::ranges::transform(text, text.begin(), to_lowercase); }

        void expand_tabs(std::u32string& text, std::size_t width) {
            std::u32string out;
            out.reserve(text.size());
            std::size_t column = 0;
            for (const char32_t code_point: text) {
                if (code_point == U'\n') {
                    out.push_back(code_point);
                    column = 0;
                } else if (code_point == U'\t') {
                    // To the next tab stop, not a fixed number of spaces: that
                    // is what a tab means and what the file looked like in the
                    // editor it came from.
                    const std::size_t stop = width == 0 ? 1 : width - (column % width);
                    out.append(stop, U' ');
                    column += stop;
                } else {
                    out.push_back(code_point);
                    ++column;
                }
            }
            text = std::move(out);
        }

    }  // namespace

    std::uint8_t combining_class(char32_t code_point) noexcept {
        using tables::CombiningClassRange;
        // libstdc++ makes a std::array iterator a pointer and MSVC makes it a
        // class; spelling it as a pointer compiles on exactly one of the two.
        // NOLINTNEXTLINE(readability-qualified-auto)
        const auto found =
                std::ranges::upper_bound(tables::kCombiningClasses, code_point, {}, &CombiningClassRange::first);
        if (found == tables::kCombiningClasses.begin()) {
            return 0;
        }
        const CombiningClassRange& range = *std::prev(found);
        return code_point <= range.last ? range.combining_class : 0;
    }

    bool is_punctuation(char32_t code_point) noexcept { return in_ranges(tables::kPunctuation, code_point); }

    char32_t to_lowercase(char32_t code_point) noexcept {
        // libstdc++ makes a std::array iterator a pointer and MSVC makes it a
        // class; spelling it as a pointer compiles on exactly one of the two.
        // NOLINTNEXTLINE(readability-qualified-auto)
        const auto found = std::ranges::lower_bound(tables::kSimpleLowercase, code_point, {}, &CaseMapping::from);
        if (found == tables::kSimpleLowercase.end() || found->from != code_point) {
            return code_point;
        }
        return found->to;
    }

    std::u32string to_nfc(std::u32string_view text) {
        // ASCII is already NFC, and ASCII is most of what gets imported. The
        // check is one pass over the text against three passes and two buffers.
        if (std::ranges::all_of(text, [](char32_t code_point) { return code_point < 0x80; })) {
            return std::u32string{text};
        }

        std::u32string decomposed;
        decomposed.reserve(text.size());
        for (const char32_t code_point: text) {
            decompose(code_point, decomposed);
        }
        order_canonically(decomposed);
        compose(decomposed);
        return decomposed;
    }

    Result<std::string> normalize(std::string_view text, const NormalizeOptions& options) {
        std::u32string points;
        points.reserve(text.size());
        for (std::size_t offset = 0; offset < text.size();) {
            const Result<DecodedCodePoint> decoded = decode_one(text, offset);
            if (!decoded) {
                return std::unexpected{decoded.error()};
            }
            points.push_back(decoded->code_point);
            offset += decoded->length;
        }

        // GAMEPLAY §5.2, in order. Reordering these changes the output — see
        // TextNormalizerTest's order fixture, which is where that is pinned.
        if (options.line_endings) {
            normalize_line_endings(points);
        }
        if (options.nfc) {
            points = to_nfc(points);
        }
        if (options.flatten_typography) {
            flatten_typography(points);
        }
        if (options.collapse_whitespace) {
            collapse_whitespace(points);
        }
        if (options.strip_punctuation) {
            strip_punctuation(points);
            if (options.collapse_whitespace) {
                // Removing a character must not put back the whitespace the
                // previous step took out. `a - b` losing its dash leaves two
                // spaces, and text that normalises differently the second time
                // is text whose hash depends on how often it was imported.
                collapse_whitespace(points);
            }
        }
        if (options.lowercase) {
            lowercase(points);
        }
        if (options.expand_tabs) {
            expand_tabs(points, options.tab_width);
        }

        std::string out;
        out.reserve(points.size());
        for (const char32_t code_point: points) {
            encode_one(code_point, out);
        }
        return out;
    }

}  // namespace typeit::core
