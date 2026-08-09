#include "typeit/app/ingest/Markdown.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        /// Reference-link definitions, keyed by their lowercased label because
        /// Markdown matches them case-insensitively.
        using Definitions = std::map<std::string, std::string, std::less<>>;

        char char_at(std::string_view text, std::size_t offset) {
            // Unchecked on purpose: every caller has just compared `offset`
            // against `text.size()`, and at() would throw, which nothing in
            // this layer does (ADR-009).
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[offset];
        }

        std::string_view line_at(std::span<const std::string_view> lines, std::size_t at) {
            // Unchecked on purpose, exactly as char_at: every caller has just
            // compared `at` against `lines.size()`.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return lines[at];
        }

        [[nodiscard]] std::string lowered(std::string_view text) {
            std::string out;
            out.reserve(text.size());
            for (const char letter: text) {
                out += static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            }
            return out;
        }

        [[nodiscard]] std::string_view trim_right(std::string_view line) {
            // Markdown's two trailing spaces are a line break, which is to say
            // two characters somebody would have to type and cannot see.
            const std::size_t last = line.find_last_not_of(" \t");
            return last == std::string_view::npos ? std::string_view{} : line.substr(0, last + 1);
        }

        [[nodiscard]] std::string_view trim_left(std::string_view line) {
            const std::size_t first = line.find_first_not_of(" \t");
            return first == std::string_view::npos ? std::string_view{} : line.substr(first);
        }

        [[nodiscard]] std::string_view trimmed(std::string_view line) { return trim_left(trim_right(line)); }

        /// The document's lines, with any carriage returns dropped.
        ///
        /// A CRLF file would otherwise leave a `\r` on the end of every line,
        /// which turns "is this line exactly `---`" into "is this line `---\r`"
        /// for every test below. Normalisation would strip them later anyway;
        /// stripping them here means the scanner only has to know one shape.
        [[nodiscard]] std::vector<std::string_view> lines_of(std::string_view text) {
            if (text.empty()) {
                return {};
            }
            std::vector<std::string_view> lines;
            std::size_t start = 0;
            while (start <= text.size()) {
                const std::size_t end = std::min(text.find('\n', start), text.size());
                std::string_view line = text.substr(start, end - start);
                if (line.ends_with('\r')) {
                    line.remove_suffix(1);
                }
                lines.push_back(line);
                start = end + 1;
            }
            // A file ending in a newline ends *one* line, not one line and an
            // empty one after it — otherwise every document gains a blank line
            // on import, and the blank line a document genuinely ends with
            // survives because it produced two empties here rather than one.
            if (lines.size() > 1 && lines.back().empty()) {
                lines.pop_back();
            }
            return lines;
        }

        // ---- block-level recognition -------------------------------------------

        /// The fence a line opens or closes, or empty.
        [[nodiscard]] std::string_view fence_of(std::string_view line) {
            const std::string_view bare = trim_left(line);
            if (bare.starts_with("```")) {
                return "```";
            }
            if (bare.starts_with("~~~")) {
                return "~~~";
            }
            return {};
        }

        /// `## Heading` → 2. Zero when the line is not an ATX heading.
        [[nodiscard]] std::size_t heading_level(std::string_view line) {
            const std::string_view bare = trim_left(line);
            const std::size_t hashes = bare.find_first_not_of('#');
            if (hashes == 0 || hashes > 6) {
                return 0;
            }
            if (hashes == std::string_view::npos || char_at(bare, hashes) != ' ') {
                // `#hashtag` is not a heading, and neither is a line of nothing
                // but hashes; the space is what makes one.
                return 0;
            }
            return hashes;
        }

        [[nodiscard]] std::string_view heading_text(std::string_view line) {
            std::string_view bare = trim_left(line);
            bare.remove_prefix(std::min(bare.find_first_not_of('#'), bare.size()));
            // A closing run of hashes is decoration: `## Title ##`.
            const std::size_t tail = bare.find_last_not_of('#');
            return trimmed(tail == std::string_view::npos ? std::string_view{} : bare.substr(0, tail + 1));
        }

        /// `---`, `***`, `___`: three or more of one mark and nothing else.
        [[nodiscard]] bool is_thematic_break(std::string_view line) {
            const std::string_view bare = trimmed(line);
            if (bare.size() < 3) {
                return false;
            }
            const char mark = char_at(bare, 0);
            return (mark == '-' || mark == '*' || mark == '_') &&
                   std::ranges::all_of(bare, [mark](char letter) { return letter == mark || letter == ' '; });
        }

        /// The `=====` or `-----` under a Setext heading, which is only a
        /// heading when there is a line of text above it to underline.
        [[nodiscard]] bool is_setext_rule(std::string_view line) {
            const std::string_view bare = trimmed(line);
            if (bare.empty()) {
                return false;
            }
            const char mark = char_at(bare, 0);
            return (mark == '=' || mark == '-') &&
                   std::ranges::all_of(bare, [mark](char letter) { return letter == mark; });
        }

        /// `- item`, `* item`, `3. item` → the item. The marker is a bullet the
        /// renderer draws, not a character anybody typed on purpose.
        [[nodiscard]] std::string_view strip_list_marker(std::string_view line) {
            const std::string_view bare = trim_left(line);
            if (bare.size() >= 2 && (bare.starts_with("- ") || bare.starts_with("* ") || bare.starts_with("+ "))) {
                return trim_left(bare.substr(2));
            }
            const std::size_t digits = bare.find_first_not_of("0123456789");
            if (digits > 0 && digits != std::string_view::npos && digits + 1 < bare.size() &&
                (char_at(bare, digits) == '.' || char_at(bare, digits) == ')') && char_at(bare, digits + 1) == ' ') {
                return trim_left(bare.substr(digits + 2));
            }
            return line;
        }

        /// `> quoted` → `quoted`, however many levels deep.
        [[nodiscard]] std::string_view strip_quote_markers(std::string_view line) {
            std::string_view bare = trim_left(line);
            while (bare.starts_with('>')) {
                bare.remove_prefix(1);
                bare = trim_left(bare);
            }
            return bare;
        }

        /// `|---|:--:|` — the row that draws the rule under a table's headers.
        [[nodiscard]] bool is_table_rule(std::string_view line) {
            const std::string_view bare = trimmed(line);
            return bare.contains('|') && bare.find_first_not_of("|-: \t") == std::string_view::npos;
        }

        /// `| a | b |` → `a b`. The pipes align columns on a screen; typing
        /// them is typing the table's frame rather than its contents.
        [[nodiscard]] std::string table_row_text(std::string_view line) {
            std::string_view bare = trimmed(line);
            if (bare.starts_with('|')) {
                bare.remove_prefix(1);
            }
            if (bare.ends_with('|')) {
                bare.remove_suffix(1);
            }

            std::string out;
            for (std::size_t start = 0; start <= bare.size();) {
                const std::size_t bar = std::min(bare.find('|', start), bare.size());
                const std::string_view cell = trimmed(bare.substr(start, bar - start));
                if (!cell.empty()) {
                    if (!out.empty()) {
                        out += ' ';
                    }
                    out += cell;
                }
                start = bar + 1;
            }
            return out;
        }

        // ---- link definitions ---------------------------------------------------

        /// `[label]: https://example.com "title"`, or nothing.
        [[nodiscard]] std::pair<std::string, std::string> definition_of(std::string_view line) {
            const std::string_view bare = trim_left(line);
            if (!bare.starts_with('[')) {
                return {};
            }
            const std::size_t close = bare.find(']');
            if (close == std::string_view::npos || close + 1 >= bare.size() || char_at(bare, close + 1) != ':') {
                return {};
            }
            return {lowered(bare.substr(1, close - 1)), std::string{trimmed(bare.substr(close + 2))}};
        }

        [[nodiscard]] Definitions definitions_in(std::span<const std::string_view> lines) {
            Definitions defs;
            for (const std::string_view line: lines) {
                if (auto [label, target] = definition_of(line); !label.empty()) {
                    // First definition wins, as Markdown says.
                    defs.emplace(std::move(label), std::move(target));
                }
            }
            return defs;
        }

        // ---- inline markup ------------------------------------------------------

        std::string strip_inline(std::string_view line, const Definitions& defs);

        /// The offset of the `]` or `)` closing what starts at `open`, counting
        /// nested pairs, or npos.
        ///
        /// The closer is derived rather than passed: two adjacent `char`
        /// parameters are two a caller can swap, and `matching(line, at, ']',
        /// '[')` would compile and silently never match anything.
        [[nodiscard]] std::size_t matching(std::string_view line, std::size_t open) {
            const char opener = char_at(line, open);
            const char closer = opener == '[' ? ']' : ')';
            std::size_t depth = 0;
            for (std::size_t at = open; at < line.size(); ++at) {
                const char letter = char_at(line, at);
                if (letter == '\\') {
                    ++at;
                } else if (letter == opener) {
                    ++depth;
                } else if (letter == closer && --depth == 0) {
                    return at;
                }
            }
            return std::string_view::npos;
        }

        /// A `[text](url)`, `[text][label]`, `[label]` or `![alt](url)` at
        /// `at`. Advances `at` past it and appends what survives.
        ///
        /// What survives is the text and never the URL: a link's text is the
        /// sentence somebody wrote, and its target is a string of slashes and
        /// percent-escapes that nobody would choose to type.
        void take_link(std::string_view line, std::size_t& at, std::string& out, const Definitions& defs) {
            const bool image = at > 0 && char_at(line, at - 1) == '!';
            const std::size_t close = matching(line, at);
            if (close == std::string_view::npos) {
                out += char_at(line, at++);
                return;
            }

            const std::string_view inner = line.substr(at + 1, close - at - 1);
            std::size_t after = close + 1;
            bool linked = false;
            if (after < line.size() && char_at(line, after) == '(') {
                const std::size_t target = matching(line, after);
                after = target == std::string_view::npos ? after : target + 1;
                linked = target != std::string_view::npos;
            } else if (after < line.size() && char_at(line, after) == '[') {
                // `[text][label]`, including the `[text][]` shorthand. An
                // unresolved label degrades to its text rather than vanishing:
                // a typo in a reference should cost the reader a link, not a
                // sentence.
                const std::size_t label = matching(line, after);
                after = label == std::string_view::npos ? after : label + 1;
                linked = label != std::string_view::npos;
            } else {
                // `[label]` on its own is a link only if something defines it;
                // otherwise the brackets are prose and stay.
                linked = defs.contains(lowered(inner));
            }

            at = after;
            if (!linked) {
                out += '[';
                out += strip_inline(inner, defs);
                out += ']';
                return;
            }
            // An image's alt text is a description of a picture for somebody
            // who cannot see it, not a sentence the author wrote to be read.
            // Typing "photograph of a lighthouse" mid-paragraph is a non
            // sequitur, so the whole image goes.
            if (image) {
                out.pop_back();  // The `!`, already emitted.
                return;
            }
            out += strip_inline(inner, defs);
        }

        /// A run of backticks and whatever they enclose, kept verbatim minus
        /// the backticks: the code inside is exactly what somebody would type.
        void take_code_span(std::string_view line, std::size_t& at, std::string& out) {
            const std::size_t ticks = std::min(line.find_first_not_of('`', at), line.size()) - at;
            const std::string fence(ticks, '`');
            const std::size_t close = line.find(fence, at + ticks);
            if (close == std::string_view::npos) {
                out.append(line.substr(at, ticks));
                at += ticks;
                return;
            }
            out.append(trimmed(line.substr(at + ticks, close - at - ticks)));
            at = close + ticks;
        }

        /// `<em>`, `<!-- note -->`, `<https://example.com>` — all of it goes.
        /// A bare `<` that opens none of those is arithmetic and stays.
        void take_angle(std::string_view line, std::size_t& at, std::string& out) {
            if (line.compare(at, 4, "<!--") == 0) {
                const std::size_t end = line.find("-->", at);
                at = end == std::string_view::npos ? line.size() : end + 3;
                return;
            }
            const std::size_t close = line.find('>', at);
            const char first = at + 1 < line.size() ? char_at(line, at + 1) : '\0';
            const bool tag = std::isalpha(static_cast<unsigned char>(first)) != 0 || first == '/' || first == '!';
            if (close == std::string_view::npos || !tag) {
                // Not a tag but arithmetic, or a comparison in prose. `a < b`
                // is a sentence and stays one.
                out += char_at(line, at++);
                return;
            }
            at = close + 1;
            out += ' ';  // So `a<br>b` does not become one word.
        }

        std::string strip_inline(std::string_view line, const Definitions& defs) {
            std::string out;
            out.reserve(line.size());
            for (std::size_t at = 0; at < line.size();) {
                const char letter = char_at(line, at);
                if (letter == '\\' && at + 1 < line.size() &&
                    std::ispunct(static_cast<unsigned char>(char_at(line, at + 1))) != 0) {
                    // An escaped mark is a mark somebody meant literally, and
                    // the backslash is the instruction rather than the text.
                    out += char_at(line, at + 1);
                    at += 2;
                } else if (letter == '`') {
                    take_code_span(line, at, out);
                } else if (letter == '[') {
                    take_link(line, at, out, defs);
                } else if (letter == '<') {
                    take_angle(line, at, out);
                } else if (letter == '*' || letter == '_') {
                    // Emphasis is a font on a screen and nothing at all in a
                    // terminal typing test. Unmatched marks go too: a stray
                    // asterisk is far likelier to be markup than arithmetic.
                    ++at;
                } else {
                    out += letter;
                    ++at;
                }
            }
            return out;
        }

        // ---- the document -------------------------------------------------------

        /// Where the body starts: past YAML `---` or TOML `+++` front matter.
        ///
        /// Front matter is a machine's metadata that happens to sit inside the
        /// file. Typing `layout: post` is typing a build system's configuration.
        [[nodiscard]] std::size_t body_start(std::span<const std::string_view> lines) {
            if (lines.empty()) {
                return 0;
            }
            const std::string_view opener = trimmed(lines.front());
            if (opener != "---" && opener != "+++") {
                return 0;
            }
            for (std::size_t at = 1; at < lines.size(); ++at) {
                if (trimmed(line_at(lines, at)) == opener) {
                    return at + 1;
                }
            }
            // An unterminated fence is a document that starts with a thematic
            // break, not one that is entirely metadata.
            return 0;
        }

        /// The output being assembled, and the sections found so far.
        class Document {
        public:
            void add_line(std::string_view text) {
                out_ += text;
                out_ += '\n';
            }

            /// Start a section here, closing whatever was open.
            void open_section(std::string title) {
                if (sections_.empty() && !trimmed(out_).empty()) {
                    // Content before the first heading belongs to a section
                    // too, or a bookmark landing in it would belong to none.
                    sections_.push_back(SectionBoundary{.title = {}, .start = 0, .length = out_.size()});
                }
                close();
                sections_.push_back(SectionBoundary{.title = std::move(title), .start = out_.size(), .length = 0});
            }

            /// A heading is both a section boundary and a line of the text —
            /// the author wrote those words for somebody to read.
            void start_heading(const std::string& title) {
                open_section(title);
                add_line(title);
            }

            ExtractedText finish() {
                close();
                ExtractedText extracted;
                extracted.text = std::move(out_);
                extracted.sections = std::move(sections_);
                return extracted;
            }

        private:
            void close() {
                if (!sections_.empty()) {
                    SectionBoundary& open = sections_.back();
                    open.length = out_.size() - open.start;
                }
            }

            std::string out_;
            std::vector<SectionBoundary> sections_;
        };

        /// Copies a fenced block verbatim and returns the line after it.
        ///
        /// Verbatim is the whole point: a code sample is the one part of a
        /// Markdown file that is *already* what somebody wants to type, and
        /// every space in it is load-bearing.
        [[nodiscard]] std::size_t take_fence(std::span<const std::string_view> lines, std::size_t at, Document& doc) {
            const std::string_view fence = fence_of(line_at(lines, at));
            for (++at; at < lines.size(); ++at) {
                if (fence_of(line_at(lines, at)) == fence) {
                    return at + 1;
                }
                doc.add_line(line_at(lines, at));
            }
            return at;
        }

        /// One ordinary line, with its block marker and inline markup removed.
        [[nodiscard]] std::string plain_line(std::string_view line, const Definitions& defs) {
            const std::string_view unquoted = strip_quote_markers(line);
            if (is_table_rule(unquoted)) {
                return {};
            }
            if (unquoted.contains('|') && trimmed(unquoted).starts_with('|')) {
                return strip_inline(table_row_text(unquoted), defs);
            }
            return strip_inline(trim_right(strip_list_marker(unquoted)), defs);
        }

        [[nodiscard]] std::string title_of(const ExtractedText& extracted, const FetchedContent& content) {
            // The first heading names the document better than its filename
            // does, and is what the author would call it.
            if (!extracted.sections.empty() && !extracted.sections.front().title.empty()) {
                return extracted.sections.front().title;
            }
            return content.suggested_title;
        }

    }  // namespace

    std::span<const std::string_view> MarkdownExtractor::mime_types() const {
        static constexpr std::array<std::string_view, 1> kTypes{"text/markdown"};
        return kTypes;
    }

    core::Result<ExtractedText> MarkdownExtractor::extract(const FetchedContent& content) const {
        const std::string_view source = as_text(content);
        if (options_.preserve_markup) {
            ExtractedText verbatim;
            verbatim.text = std::string{source};
            if (!content.suggested_title.empty()) {
                verbatim.title = content.suggested_title;
            }
            return verbatim;
        }

        const std::vector<std::string_view> lines = lines_of(source);
        const Definitions defs = definitions_in(lines);

        Document doc;
        for (std::size_t at = body_start(lines); at < lines.size();) {
            const std::string_view line = line_at(lines, at);
            if (!fence_of(line).empty()) {
                at = take_fence(lines, at, doc);
            } else if (heading_level(line) > 0) {
                doc.start_heading(strip_inline(heading_text(line), defs));
                ++at;
            } else if (at + 1 < lines.size() && !trimmed(line).empty() && is_setext_rule(line_at(lines, at + 1)) &&
                       !is_thematic_break(line)) {
                // A line of text underlined with `===` or `---` is a Setext
                // heading, and only the line above turns that rule into one.
                doc.start_heading(strip_inline(trimmed(line), defs));
                at += 2;
            } else if (is_thematic_break(line) || !definition_of(line).first.empty()) {
                ++at;
            } else {
                doc.add_line(plain_line(line, defs));
                ++at;
            }
        }

        ExtractedText extracted = doc.finish();
        if (std::string title = title_of(extracted, content); !title.empty()) {
            extracted.title = std::move(title);
        }
        return extracted;
    }

}  // namespace typeit::app
