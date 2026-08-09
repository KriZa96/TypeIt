#include "typeit/app/ingest/Readiness.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        /// A line, and where in the input it came from.
        ///
        /// The provenance travels with the text because chapter boundaries are
        /// line numbers into the input, and this pass deletes and merges lines
        /// (TX-005).
        struct Line {
            std::string text;
            std::size_t source = 0;
        };

        // ---- small helpers --------------------------------------------------------

        [[nodiscard]] std::string_view trimmed(std::string_view line) {
            const std::size_t first = line.find_first_not_of(" \t\r");
            if (first == std::string_view::npos) {
                return {};
            }
            return line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        }

        [[nodiscard]] bool is_ascii_letter(char letter) {
            return std::isalpha(static_cast<unsigned char>(letter)) != 0;
        }

        [[nodiscard]] bool is_ascii_digit(char letter) { return std::isdigit(static_cast<unsigned char>(letter)) != 0; }

        [[nodiscard]] std::string lowercased(std::string_view word) {
            std::string out;
            out.reserve(word.size());
            for (const char letter: word) {
                out += static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            }
            return out;
        }

        /// Everything a byte offset needs: `text[at]` without the unchecked
        /// index clang-tidy objects to, after the caller has already compared.
        [[nodiscard]] char char_at(std::string_view text, std::size_t at) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[at];
        }

        [[nodiscard]] std::vector<Line> lines_of(std::string_view text) {
            std::vector<Line> lines;
            if (text.empty()) {
                // No lines, rather than one empty one. An empty text joined back
                // together must still be empty, or every pass over it adds a
                // newline and the idempotence promise is gone on the file it is
                // easiest to keep.
                return lines;
            }
            std::size_t start = 0;
            while (start <= text.size()) {
                const std::size_t end = std::min(text.find('\n', start), text.size());
                lines.push_back(Line{.text = std::string{text.substr(start, end - start)}, .source = lines.size()});
                start = end + 1;
            }
            // A text ending in a newline is not a text with an empty last line.
            // Splitting says otherwise, and putting the blank back would add one
            // on every pass — which is the whole idempotence promise gone.
            if (!lines.empty() && lines.back().text.empty()) {
                lines.pop_back();
            }
            return lines;
        }

        /// Back into one string, ending the way the input did.
        ///
        /// A text without a trailing newline must come back without one. Adding
        /// it would be one extra grapheme to type on every import that changed
        /// nothing else, which is a behaviour change dressed up as a clean-up.
        [[nodiscard]] std::string joined(const std::vector<Line>& lines, bool trailing_newline) {
            std::string out;
            for (const Line& line: lines) {
                if (!out.empty()) {
                    out += '\n';
                }
                out += line.text;
            }
            if (trailing_newline && !out.empty()) {
                out += '\n';
            }
            return out;
        }

        // ---- Project Gutenberg ----------------------------------------------------

        /// The formulaic marker, in either of the two wordings Gutenberg has
        /// used. Recognised by the stars *and* the name, because a line that
        /// merely says "project gutenberg" is a sentence about the archive and
        /// somebody may well want to type it.
        enum class Boundary : std::uint8_t { Start, End };

        [[nodiscard]] bool is_gutenberg_marker(std::string_view line, Boundary which) {
            const std::string upper = [line] {
                std::string out;
                for (const char letter: line) {
                    out += static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
                }
                return out;
            }();
            const std::string_view bare = trimmed(upper);
            return bare.starts_with("***") && bare.contains("PROJECT GUTENBERG") &&
                   bare.contains(which == Boundary::Start ? "START" : "END");
        }

        [[nodiscard]] std::vector<Line> without_boilerplate(std::vector<Line> lines, ReadinessReport& report) {
            std::size_t first = 0;
            std::size_t last = lines.size();
            for (std::size_t at = 0; at < lines.size(); ++at) {
                const std::string_view line = lines.at(at).text;
                if (first == 0 && is_gutenberg_marker(line, Boundary::Start)) {
                    first = at + 1;
                } else if (last == lines.size() && first > 0 && is_gutenberg_marker(line, Boundary::End)) {
                    last = at;
                }
            }
            if (first == 0) {
                return lines;
            }
            report.boilerplate_removed = true;
            return {lines.begin() + static_cast<std::ptrdiff_t>(first),
                    lines.begin() + static_cast<std::ptrdiff_t>(last)};
        }

        // ---- table of contents ----------------------------------------------------

        /// How far into a document a dotted-leader line is still plausibly a
        /// table of contents rather than a table.
        constexpr std::size_t kContentsWindow = 50;

        /// `Chapter One .......... 14`: text, a run of dots, a page number.
        [[nodiscard]] bool is_dotted_leader(std::string_view line) {
            const std::string_view bare = trimmed(line);
            if (bare.size() < 5 || !is_ascii_digit(bare.back()) || !bare.contains("...")) {
                return false;
            }
            // The dots have to come before the number, or this is a sentence
            // trailing off into an ellipsis followed by a year.
            const std::size_t dots = bare.find("...");
            const std::size_t digits = bare.find_last_not_of("0123456789");
            return digits != std::string_view::npos && dots < digits;
        }

        [[nodiscard]] std::vector<Line> without_table_of_contents(std::vector<Line> lines, ReadinessReport& report) {
            const std::size_t window = std::min(kContentsWindow, lines.size());
            std::vector<Line> kept;
            kept.reserve(lines.size());
            bool started = false;
            bool finished = false;
            for (std::size_t at = 0; at < lines.size(); ++at) {
                Line& line = lines.at(at);
                const bool leader = at < window && !finished && is_dotted_leader(line.text);
                if (leader) {
                    started = true;
                    ++report.contents_lines_dropped;
                    continue;
                }
                // Blank lines inside the block go with it; the first real line
                // after it ends the block, so a dotted leader appearing later in
                // the book is a table and is left alone.
                if (started && trimmed(line.text).empty()) {
                    continue;
                }
                finished = started;
                kept.push_back(std::move(line));
            }
            return kept;
        }

        // ---- running heads and page numbers ---------------------------------------

        /// Longer than this and it is a sentence, not a header.
        constexpr std::size_t kRunningHeadLength = 60;
        /// Fewer than this and it is somebody repeating themselves.
        constexpr std::size_t kRunningHeadRepeats = 3;

        [[nodiscard]] bool ends_a_sentence(std::string_view line) {
            const std::string_view bare = trimmed(line);
            return !bare.empty() && (bare.back() == '.' || bare.back() == '!' || bare.back() == '?');
        }

        /// A line that is nothing but a page number, in any of the usual
        /// dressings: `14`, `- 14 -`, `[14]`, `Page 14`.
        [[nodiscard]] bool is_page_number(std::string_view line) {
            std::string_view bare = trimmed(line);
            if (bare.size() > 12) {
                return false;
            }
            const std::string lower = lowercased(bare);
            if (lower.starts_with("page ")) {
                bare = bare.substr(5);
                bare = trimmed(bare);
            }
            while (!bare.empty() && (bare.front() == '-' || bare.front() == '[' || bare.front() == '(')) {
                bare.remove_prefix(1);
            }
            while (!bare.empty() && (bare.back() == '-' || bare.back() == ']' || bare.back() == ')')) {
                bare.remove_suffix(1);
            }
            bare = trimmed(bare);
            return !bare.empty() && std::ranges::all_of(bare, is_ascii_digit);
        }

        [[nodiscard]] std::vector<Line> without_running_heads(std::vector<Line> lines, ReadinessReport& report) {
            std::map<std::string, std::size_t, std::less<>> repeats;
            for (const Line& line: lines) {
                const std::string_view bare = trimmed(line.text);
                if (!bare.empty() && bare.size() <= kRunningHeadLength && !ends_a_sentence(bare)) {
                    ++repeats[std::string{bare}];
                }
            }

            std::vector<Line> kept;
            kept.reserve(lines.size());
            bool swallow_blank = false;
            for (Line& line: lines) {
                const std::string bare{trimmed(line.text)};
                if (swallow_blank && bare.empty()) {
                    // The page number sat alone between two blank lines, so
                    // removing it would otherwise leave a double paragraph
                    // break — once per page, right through the book.
                    swallow_blank = false;
                    continue;
                }
                swallow_blank = false;
                // A page number goes on sight. A running head has to prove
                // itself by turning up again: a chapter title appears once and
                // somebody wants to type it.
                const auto found = repeats.find(bare);
                const bool repeated = found != repeats.end() && found->second >= kRunningHeadRepeats;
                if (!bare.empty() && (is_page_number(bare) || repeated)) {
                    ++report.running_heads_dropped;
                    swallow_blank = !kept.empty() && trimmed(kept.back().text).empty();
                    continue;
                }
                kept.push_back(std::move(line));
            }
            return kept;
        }

        // ---- footnote markers -----------------------------------------------------

        /// The superscript digits, which is how a typeset footnote marker
        /// arrives when a PDF or an EPUB kept its styling.
        ///
        /// Two lengths, because Unicode put them in two places: `¹²³` are the
        /// Latin-1 leftovers at two bytes each and the rest are in the
        /// superscripts block at three. Checking only the longer one is how the
        /// first cut of this let every `¹` through.
        [[nodiscard]] std::size_t superscript_digit_length(std::string_view text) {
            static const std::set<std::string, std::less<>> kTwoByte{"¹", "²", "³"};
            static const std::set<std::string, std::less<>> kThreeByte{"⁰", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
            if (kTwoByte.contains(text.substr(0, 2))) {
                return 2;
            }
            return kThreeByte.contains(text.substr(0, 3)) ? 3 : 0;
        }

        [[nodiscard]] std::string without_footnote_markers(std::string_view line, ReadinessReport& report) {
            std::string out;
            out.reserve(line.size());
            for (std::size_t at = 0; at < line.size();) {
                // `[12]` and nothing else: `[sic]` and `[a]` are things people
                // write in prose, and a bracket with words in it survives.
                if (char_at(line, at) == '[') {
                    std::size_t scan = at + 1;
                    while (scan < line.size() && is_ascii_digit(char_at(line, scan))) {
                        ++scan;
                    }
                    if (scan > at + 1 && scan < line.size() && char_at(line, scan) == ']') {
                        ++report.footnote_markers_stripped;
                        at = scan + 1;
                        continue;
                    }
                }
                if (const std::size_t superscript = superscript_digit_length(line.substr(at)); superscript > 0) {
                    ++report.footnote_markers_stripped;
                    at += superscript;
                    continue;
                }
                out += char_at(line, at);
                ++at;
            }
            return out;
        }

        // ---- dehyphenation --------------------------------------------------------

        /// Every word the document uses, which is the only dictionary actually
        /// available here.
        ///
        /// ponytail: the document is the dictionary. A bundled word list would
        /// be one language's, would need generating and guarding like the
        /// Unicode tables, and would still be wrong about names and jargon —
        /// which is most of what a hyphen at a line break lands in. Swap in a
        /// real word list if the fallback below turns out to be wrong often.
        class Vocabulary {
        public:
            explicit Vocabulary(const std::vector<Line>& lines) {
                for (std::size_t at = 0; at < lines.size(); ++at) {
                    const std::string_view line = lines.at(at).text;
                    // The two words either side of a line-break hyphen are
                    // excluded: `ple` is not evidence that `ple` is a word, and
                    // believing it would keep exactly the hyphens this is
                    // trying to remove.
                    const bool breaks = trimmed(line).ends_with('-');
                    const bool after_break = at > 0 && trimmed(lines.at(at - 1).text).ends_with('-');
                    add_words(line, breaks, after_break);
                }
            }

            [[nodiscard]] bool knows(std::string_view word) const { return words_.contains(lowercased(word)); }

        private:
            void add_words(std::string_view line, bool skip_last, bool skip_first) {
                std::vector<std::string> found;
                std::string word;
                for (const char letter: line) {
                    if (is_ascii_letter(letter) || (letter == '-' && !word.empty())) {
                        word += letter;
                        continue;
                    }
                    if (!word.empty()) {
                        found.push_back(std::exchange(word, {}));
                    }
                }
                if (!word.empty()) {
                    found.push_back(std::move(word));
                }
                for (std::size_t at = 0; at < found.size(); ++at) {
                    if ((skip_first && at == 0) || (skip_last && at + 1 == found.size())) {
                        continue;
                    }
                    std::string_view token = found.at(at);
                    while (token.ends_with('-')) {
                        token.remove_suffix(1);
                    }
                    if (!token.empty()) {
                        words_.insert(lowercased(token));
                    }
                }
            }

            std::set<std::string, std::less<>> words_;
        };

        /// The word ending at a trailing hyphen, and the one starting the next
        /// line.
        [[nodiscard]] std::string_view trailing_word(std::string_view line) {
            std::size_t start = line.size();
            while (start > 0 && is_ascii_letter(char_at(line, start - 1))) {
                --start;
            }
            return line.substr(start);
        }

        [[nodiscard]] std::string_view leading_word(std::string_view line) {
            std::size_t end = 0;
            while (end < line.size() && is_ascii_letter(char_at(line, end))) {
                ++end;
            }
            return line.substr(0, end);
        }

        /// Whether the hyphen was the typesetter's or the author's.
        ///
        /// In descending order of evidence: the document spells the word joined
        /// somewhere else, the document spells it hyphenated somewhere else,
        /// both halves are words the document uses on their own. Failing all
        /// three it goes, because a hyphen landing exactly at a line ending is
        /// far more often a line break than a compound.
        [[nodiscard]] bool hyphen_was_the_authors(const Vocabulary& vocabulary, std::string_view head,
                                                  std::string_view tail) {
            const std::string joined_word = std::string{head} + std::string{tail};
            if (vocabulary.knows(joined_word)) {
                return false;
            }
            if (vocabulary.knows(std::string{head} + "-" + std::string{tail})) {
                return true;
            }
            return vocabulary.knows(head) && vocabulary.knows(tail);
        }

        [[nodiscard]] std::vector<Line> dehyphenated(std::vector<Line> lines, ReadinessReport& report) {
            const Vocabulary vocabulary{lines};

            std::vector<Line> kept;
            kept.reserve(lines.size());
            for (Line& line: lines) {
                const std::string_view bare = trimmed(line.text);
                const bool joins = !kept.empty() && trimmed(kept.back().text).ends_with('-') && !bare.empty() &&
                                   is_ascii_letter(bare.front());
                if (!joins) {
                    kept.push_back(std::move(line));
                    continue;
                }

                std::string& previous = kept.back().text;
                previous.resize(std::string_view{previous}.find_last_of('-'));
                const std::string_view head = trailing_word(previous);
                const std::string_view tail = leading_word(bare);
                // A hyphen with nothing before it is punctuation somebody typed,
                // not a word broken in half.
                if (head.empty() || tail.empty()) {
                    previous += '-';
                    kept.push_back(std::move(line));
                    continue;
                }
                if (hyphen_was_the_authors(vocabulary, head, tail)) {
                    previous += '-';
                }
                previous += bare;
                ++report.dehyphenated;
            }
            return kept;
        }

        // ---- paragraph rejoin -----------------------------------------------------

        /// Below this, a block's lines were meant to be that short: poetry, a
        /// list, an address, a stack of headings.
        constexpr std::size_t kWrappedLineLength = 45;

        [[nodiscard]] bool starts_a_list_item(std::string_view line) {
            const std::string_view bare = trimmed(line);
            if (bare.size() < 2) {
                return false;
            }
            if ((bare.front() == '-' || bare.front() == '*' || bare.front() == '+') && char_at(bare, 1) == ' ') {
                return true;
            }
            std::size_t at = 0;
            while (at < bare.size() && is_ascii_digit(char_at(bare, at))) {
                ++at;
            }
            return at > 0 && at + 1 < bare.size() && (char_at(bare, at) == '.' || char_at(bare, at) == ')') &&
                   char_at(bare, at + 1) == ' ';
        }

        /// Whether a run of lines looks like a hard-wrapped paragraph.
        ///
        /// The median rather than the mean, because a paragraph's last line is
        /// short by definition and a mean over three lines is half decided by
        /// it.
        [[nodiscard]] bool looks_hard_wrapped(std::span<const Line> block) {
            if (block.size() < 2) {
                return false;
            }
            std::vector<std::size_t> lengths;
            lengths.reserve(block.size());
            for (const Line& line: block) {
                lengths.push_back(trimmed(line.text).size());
            }
            std::ranges::sort(lengths);
            return lengths.at(lengths.size() / 2) >= kWrappedLineLength;
        }

        void rejoin_block(std::span<Line> block, std::vector<Line>& out, ReadinessReport& report) {
            if (!looks_hard_wrapped(block)) {
                for (Line& line: block) {
                    out.push_back(std::move(line));
                }
                return;
            }
            for (Line& line: block) {
                if (out.empty() || out.back().text.empty() || starts_a_list_item(line.text)) {
                    out.push_back(std::move(line));
                    continue;
                }
                out.back().text += ' ';
                out.back().text += trimmed(line.text);
                ++report.lines_rejoined;
            }
        }

        [[nodiscard]] std::vector<Line> with_paragraphs_rejoined(std::vector<Line> lines, ReadinessReport& report) {
            std::vector<Line> out;
            out.reserve(lines.size());
            std::size_t start = 0;
            for (std::size_t at = 0; at <= lines.size(); ++at) {
                const bool blank = at == lines.size() || trimmed(lines.at(at).text).empty();
                if (!blank) {
                    continue;
                }
                if (at > start) {
                    // A fresh vector per block so `out.back()` cannot reach into
                    // the block before this one across a blank line.
                    std::vector<Line> block;
                    rejoin_block(std::span{lines}.subspan(start, at - start), block, report);
                    for (Line& line: block) {
                        out.push_back(std::move(line));
                    }
                }
                if (at < lines.size()) {
                    out.push_back(std::move(lines.at(at)));
                }
                start = at + 1;
            }
            return out;
        }

        // ---- what a keyboard cannot reach ------------------------------------------

        [[nodiscard]] bool is_typeable_ascii(std::string_view grapheme) {
            return std::ranges::all_of(grapheme, [](char byte) {
                const auto value = static_cast<unsigned char>(byte);
                return (value >= 0x20 && value <= 0x7E) || byte == '\n' || byte == '\t';
            });
        }

        [[nodiscard]] std::vector<UnreachableGrapheme> unreachable_in(std::string_view text,
                                                                      const core::NormalizeOptions& normalization) {
            const core::Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(text);
            if (!buffer) {
                // Not this pass's error to report. Normalisation validates the
                // UTF-8 and names the byte, and a second opinion here would be
                // one with a worse message.
                return {};
            }

            std::map<std::string, std::size_t, std::less<>> counts;
            for (const core::Grapheme& grapheme: buffer->graphemes()) {
                if (!is_typeable_ascii(grapheme.view())) {
                    ++counts[std::string{grapheme.view()}];
                }
            }

            std::vector<UnreachableGrapheme> found;
            found.reserve(counts.size());
            for (const auto& [text_of, count]: counts) {
                // Asked of the real normaliser rather than of a second copy of
                // its table: a list here that drifted from the one that runs
                // would tell somebody an em dash is handled when it is not.
                const core::Result<std::string> flattened = core::normalize(text_of, normalization);
                found.push_back(
                        UnreachableGrapheme{.text = text_of,
                                            .count = count,
                                            .flattened = flattened.has_value() && is_typeable_ascii(*flattened)});
            }
            std::ranges::sort(found, [](const UnreachableGrapheme& left, const UnreachableGrapheme& right) {
                return left.count != right.count ? left.count > right.count : left.text < right.text;
            });
            return found;
        }

    }  // namespace

    ReadinessOptions ReadinessOptions::none() {
        return ReadinessOptions{.dehyphenate = false,
                                .drop_running_heads = false,
                                .strip_footnote_markers = false,
                                .drop_table_of_contents = false,
                                .rejoin_paragraphs = false,
                                .strip_boilerplate = false};
    }

    std::size_t ReadinessReport::unreachable_after_normalisation() const {
        std::size_t total = 0;
        for (const UnreachableGrapheme& grapheme: unreachable) {
            if (!grapheme.flattened) {
                total += grapheme.count;
            }
        }
        return total;
    }

    bool ReadinessReport::empty() const {
        return dehyphenated == 0 && running_heads_dropped == 0 && footnote_markers_stripped == 0 &&
               contents_lines_dropped == 0 && lines_rejoined == 0 && !boilerplate_removed && unreachable.empty();
    }

    std::vector<std::string> ReadinessReport::lines() const {
        std::vector<std::string> out;
        const auto say = [&out](std::size_t count, std::string_view one, std::string_view many) {
            if (count > 0) {
                out.push_back(std::to_string(count) + " " + std::string{count == 1 ? one : many});
            }
        };
        if (boilerplate_removed) {
            out.emplace_back("Project Gutenberg's header and footer removed");
        }
        say(contents_lines_dropped, "table-of-contents line dropped", "table-of-contents lines dropped");
        say(running_heads_dropped, "running head or page number dropped", "running heads and page numbers dropped");
        say(footnote_markers_stripped, "footnote marker stripped", "footnote markers stripped");
        say(dehyphenated, "word rejoined across a line break", "words rejoined across line breaks");
        say(lines_rejoined, "hard-wrapped line reflowed", "hard-wrapped lines reflowed");
        if (const std::size_t remaining = unreachable_after_normalisation(); remaining > 0) {
            // The number that decides whether this text is typeable at all, so
            // it names the worst offender rather than only counting.
            const auto worst = std::ranges::find_if(
                    unreachable, [](const UnreachableGrapheme& grapheme) { return !grapheme.flattened; });
            out.push_back(std::to_string(remaining) + " characters a standard keyboard cannot reach remain, such as " +
                          worst->text);
        }
        return out;
    }

    std::size_t ReadinessResult::line_from_source(std::size_t line) const {
        // The last output line that began at or before it: a line merged into
        // the middle of a paragraph belongs to that paragraph, and a line that
        // was deleted belongs to whatever came after it.
        std::size_t found = 0;
        for (std::size_t at = 0; at < source_lines.size(); ++at) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            if (source_lines[at] > line) {
                break;
            }
            found = at;
        }
        return found;
    }

    ReadinessResult make_typeable(std::string_view text, const ReadinessOptions& options,
                                  const core::NormalizeOptions& normalization) {
        ReadinessResult result;
        const bool trailing_newline = text.ends_with('\n');
        std::vector<Line> lines = lines_of(text);

        // The order is load-bearing and is pinned by test rather than by this
        // comment. Boilerplate first because everything after it would
        // otherwise be deciding things about a licence; dehyphenation before
        // rejoin, because a hyphen at a line ending is only recognisable while
        // the line ending is still there.
        if (options.strip_boilerplate) {
            lines = without_boilerplate(std::move(lines), result.report);
        }
        if (options.drop_table_of_contents) {
            lines = without_table_of_contents(std::move(lines), result.report);
        }
        if (options.drop_running_heads) {
            lines = without_running_heads(std::move(lines), result.report);
        }
        if (options.strip_footnote_markers) {
            for (Line& line: lines) {
                line.text = without_footnote_markers(line.text, result.report);
            }
        }
        if (options.dehyphenate) {
            lines = dehyphenated(std::move(lines), result.report);
        }
        if (options.rejoin_paragraphs) {
            lines = with_paragraphs_rejoined(std::move(lines), result.report);
        }

        result.source_lines.reserve(lines.size());
        for (const Line& line: lines) {
            result.source_lines.push_back(line.source);
        }
        result.text = joined(lines, trailing_newline);
        result.report.unreachable = unreachable_in(result.text, normalization);
        return result;
    }

}  // namespace typeit::app
