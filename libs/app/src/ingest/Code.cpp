#include "typeit/app/ingest/Code.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/text/Utf8.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        char char_at(std::string_view text, std::size_t offset) {
            // Unchecked on purpose: every caller has just compared `offset`
            // against `text.size()`, and at() would throw, which nothing in
            // this layer does (ADR-009).
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[offset];
        }

        struct Language {
            std::string_view extension;
            std::string_view name;
        };

        /// Extension to language. Not a complete list and not trying to be:
        /// this decides which comment marker to look for and which section
        /// heuristic to run, so a language nobody has taught it still imports
        /// with its whitespace intact and no sections, which is the correct
        /// answer rather than a degraded one.
        [[nodiscard]] std::span<const Language> languages() {
            static constexpr std::array<Language, 18> kKnown{{
                    {.extension = "c", .name = "c"},
                    {.extension = "h", .name = "c"},
                    {.extension = "cpp", .name = "cpp"},
                    {.extension = "cc", .name = "cpp"},
                    {.extension = "cxx", .name = "cpp"},
                    {.extension = "hpp", .name = "cpp"},
                    {.extension = "rs", .name = "rust"},
                    {.extension = "py", .name = "python"},
                    {.extension = "pyi", .name = "python"},
                    {.extension = "js", .name = "javascript"},
                    {.extension = "mjs", .name = "javascript"},
                    {.extension = "ts", .name = "typescript"},
                    {.extension = "tsx", .name = "typescript"},
                    {.extension = "go", .name = "go"},
                    {.extension = "java", .name = "java"},
                    {.extension = "cs", .name = "csharp"},
                    {.extension = "sh", .name = "shell"},
                    {.extension = "rb", .name = "ruby"},
            }};
            return kKnown;
        }

        /// Whether `#` starts a comment in this language rather than `//`.
        [[nodiscard]] bool hash_comments(std::string_view language) {
            return language == "python" || language == "shell" || language == "ruby";
        }

        /// Whether a block is delimited by braces rather than by indentation.
        [[nodiscard]] bool braced(std::string_view language) {
            return !language.empty() && language != "python" && language != "ruby" && language != "shell";
        }

        [[nodiscard]] std::vector<std::string_view> lines_of(std::string_view text) {
            std::vector<std::string_view> lines;
            std::size_t start = 0;
            while (start <= text.size()) {
                const std::size_t end = std::min(text.find('\n', start), text.size());
                std::string_view line = text.substr(start, end - start);
                // CRLF is stripped here, not by trimming trailing whitespace
                // later: a `\r` is a line ending rather than indentation, and
                // conflating them would make "keep trailing whitespace" mean
                // "keep a carriage return in the middle of the text".
                if (line.ends_with('\r')) {
                    line.remove_suffix(1);
                }
                lines.push_back(line);
                start = end + 1;
            }
            if (lines.size() > 1 && lines.back().empty()) {
                lines.pop_back();
            }
            return lines;
        }

        // ---- diagnostics --------------------------------------------------------

        /// The indentation a line opens with, as it was written.
        [[nodiscard]] std::string_view indent_of(std::string_view line) {
            const std::size_t first = line.find_first_not_of(" \t");
            return line.substr(0, first == std::string_view::npos ? line.size() : first);
        }

        /// Whether the file indents with tabs in one place and spaces in
        /// another.
        ///
        /// A file that mixes them is effectively untypeable: the two are
        /// invisible and identical on screen, so the typist has no way to know
        /// which one the next line wants. Blank lines are excluded — a stray
        /// space on an empty line is not an indentation style.
        [[nodiscard]] bool mixes_indentation(std::span<const std::string_view> lines) {
            bool tabs = false;
            bool spaces = false;
            for (const std::string_view line: lines) {
                if (line.find_first_not_of(" \t") == std::string_view::npos) {
                    continue;
                }
                const std::string_view indent = indent_of(line);
                tabs = tabs || indent.contains('\t');
                spaces = spaces || indent.contains(' ');
            }
            return tabs && spaces;
        }

        /// The UTF-8 the file is supposed to be, or the byte that says it is
        /// not.
        ///
        /// Checked here rather than left to normalisation because everything
        /// this extractor reports — the indentation, the sections, the line
        /// lengths — is a statement about a text. Saying "line 40 mixes tabs
        /// and spaces" about a file that turns out to be a JPEG is a confident
        /// answer to the wrong question.
        [[nodiscard]] core::Status valid_utf8(std::string_view text) {
            for (std::size_t at = 0; at < text.size();) {
                const core::Result<core::DecodedCodePoint> decoded = core::decode_one(text, at);
                if (!decoded) {
                    return std::unexpected{decoded.error()};
                }
                at += decoded->length;
            }
            return {};
        }

        // ---- comments -----------------------------------------------------------

        /// How far into `line` the code runs before a comment starts, given
        /// whether a block comment was already open.
        ///
        /// String-literal aware, because it has to be: `"http://example.com"`
        /// contains a `//` that starts no comment, and a stripper that does not
        /// know it is inside a string cuts the line in half. Escapes are
        /// honoured for the same reason — `"a\"//b"` is one string.
        class CommentScanner {
        public:
            CommentScanner(std::string_view language, bool hash) : hash_{hash}, block_{braced(language)} {}

            /// The code part of this line, with the comments removed.
            ///
            /// Assembled rather than returned as a prefix of the line. A
            /// prefix is enough right up until a block comment spans lines:
            /// the second line of `/* note\nstill note */ x = 1;` has its
            /// comment at the *front*, and the only correct answer is built
            /// from what is left over.
            [[nodiscard]] std::string code_in(std::string_view line) {
                std::string out;
                std::size_t at = 0;
                while (at < line.size()) {
                    if (in_block_) {
                        const std::size_t close = line.find("*/", at);
                        if (close == std::string_view::npos) {
                            return out;
                        }
                        in_block_ = false;
                        at = close + 2;
                        continue;
                    }
                    if (quote_ != '\0') {
                        const std::size_t end = past_string(line, at);
                        out.append(line.substr(at, end - at));
                        at = end;
                        continue;
                    }
                    if (starts_comment(line, at)) {
                        return out;
                    }
                    if (opens_block(line, at)) {
                        at += 2;
                        continue;
                    }
                    const char letter = char_at(line, at);
                    if (letter == '"' || letter == '\'') {
                        quote_ = letter;
                    }
                    out += letter;
                    ++at;
                }
                return out;
            }

        private:
            /// A line comment starts here and everything after it goes.
            [[nodiscard]] bool starts_comment(std::string_view line, std::size_t at) const {
                if (hash_ && char_at(line, at) == '#') {
                    return true;
                }
                return block_ && at + 1 < line.size() && char_at(line, at) == '/' && char_at(line, at + 1) == '/';
            }

            /// A block comment opens here. Recorded, because it may not close
            /// on this line.
            [[nodiscard]] bool opens_block(std::string_view line, std::size_t at) {
                if (!block_ || at + 1 >= line.size() || char_at(line, at) != '/' || char_at(line, at + 1) != '*') {
                    return false;
                }
                in_block_ = true;
                return true;
            }

            /// Past the end of the string literal that is currently open, or to
            /// the end of the line if it does not close here.
            [[nodiscard]] std::size_t past_string(std::string_view line, std::size_t at) {
                while (at < line.size()) {
                    if (char_at(line, at) == '\\') {
                        at += 2;
                        continue;
                    }
                    if (char_at(line, at) == quote_) {
                        quote_ = '\0';
                        return at + 1;
                    }
                    ++at;
                }
                // An unterminated literal does not carry to the next line. A
                // raw string could, but a scanner that gets that wrong stays
                // wrong for the rest of the file, and being wrong about one
                // line is the cheaper failure.
                quote_ = '\0';
                return at;
            }

            char quote_ = '\0';
            bool in_block_ = false;
            bool hash_ = false;
            bool block_ = false;
        };

        // ---- sections -----------------------------------------------------------

        /// Whether a top-level line looks like the start of a definition.
        ///
        /// A heuristic and labelled as one. In a braced language a definition
        /// is a line at column zero that opens a block; in Python it is `def`
        /// or `class` at column zero. Both are wrong about something — a
        /// top-level `if` in a script, a decorator above a `def` — and both are
        /// right about the shape of essentially every file anybody imports.
        [[nodiscard]] bool starts_definition(std::string_view line, std::string_view language) {
            if (line.empty() || !indent_of(line).empty() || language.empty()) {
                // An unknown language gets no sections rather than guessed
                // ones. A file this cannot name still imports with its
                // whitespace intact, which is the right answer rather than a
                // degraded one.
                return false;
            }
            const std::string_view code = line;
            if (!braced(language)) {
                return code.starts_with("def ") || code.starts_with("class ") || code.starts_with("async def ");
            }
            // A preprocessor line, a closing brace and a lone attribute are all
            // at column zero and none of them names anything.
            if (code.starts_with('#') || code.starts_with('}') || code.starts_with("//")) {
                return false;
            }
            return code.ends_with('{') || code.ends_with(')') || code.contains('(');
        }

        /// The definition's name, near enough: the line without its opening
        /// brace and its surrounding space.
        [[nodiscard]] std::string definition_title(std::string_view line) {
            std::string_view title = line;
            while (!title.empty() && (title.back() == '{' || title.back() == ' ' || title.back() == '\t')) {
                title.remove_suffix(1);
            }
            return std::string{title};
        }

        /// Sections over the assembled text, one per top-level definition.
        [[nodiscard]] std::vector<TextSection> sections_of(std::span<const std::string> emitted,
                                                           std::string_view language) {
            std::vector<TextSection> sections;
            std::size_t offset = 0;
            for (const std::string& line: emitted) {
                if (starts_definition(line, language)) {
                    if (!sections.empty()) {
                        sections.back().length = offset - sections.back().start;
                    }
                    sections.push_back(TextSection{.title = definition_title(line), .start = offset, .length = 0});
                }
                offset += line.size() + 1;  // The newline this line is joined with.
            }
            if (!sections.empty()) {
                sections.back().length = offset - sections.back().start;
            }
            return sections;
        }

        /// Normalisation that leaves a source file alone.
        [[nodiscard]] core::NormalizeOptions code_normalization() {
            core::NormalizeOptions options;
            // The two that would destroy the file. Collapsing turns every
            // indented line into one leading space; expanding turns every tab
            // into spaces, which is the opposite of "tabs kept as tabs when the
            // file uses them".
            options.collapse_whitespace = false;
            options.expand_tabs = false;
            // A curly quote inside a string literal is part of the program, and
            // an em dash inside one is a character somebody chose. Flattening
            // them edits the code.
            options.flatten_typography = false;
            // Line endings and NFC stay on: CRLF to LF is what "normalises
            // without corrupting indentation" means, and NFC is about two
            // copies of one file hashing alike rather than about the bytes a
            // typist sees.
            return options;
        }

    }  // namespace

    std::string language_of(std::string_view path) {
        const std::filesystem::path named{path};
        std::string extension = named.extension().string();
        if (extension.empty()) {
            return {};
        }
        extension.erase(0, 1);
        for (char& letter: extension) {
            letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        }

        const auto found = std::ranges::find(languages(), extension, &Language::extension);
        return found == languages().end() ? std::string{} : std::string{found->name};
    }

    std::span<const std::string_view> CodeExtractor::mime_types() const {
        static constexpr std::array<std::string_view, 1> kTypes{"text/x-code"};
        return kTypes;
    }

    core::Result<ExtractedText> CodeExtractor::extract(const FetchedContent& content) const {
        const std::string_view source = as_text(content);
        if (const core::Status valid = valid_utf8(source); !valid) {
            return std::unexpected{valid.error()};
        }

        const std::string language = language_of(content.origin);
        const std::vector<std::string_view> lines = lines_of(source);

        CommentScanner scanner{language, hash_comments(language)};
        std::vector<std::string> kept;
        kept.reserve(lines.size());
        for (const std::string_view line: lines) {
            std::string emitted = options_.strip_comments ? scanner.code_in(line) : std::string{line};
            if (!options_.keep_trailing_whitespace || (options_.strip_comments && emitted.size() < line.size())) {
                // Stripping a trailing comment leaves behind the space that
                // separated it from the code, which is trailing whitespace
                // nobody wrote on purpose.
                emitted.erase(emitted.find_last_not_of(" \t") + 1);
            }
            kept.push_back(std::move(emitted));
        }

        ExtractedText extracted;
        for (const std::string& line: kept) {
            extracted.text += line;
            extracted.text += '\n';
        }
        extracted.sections = sections_of(kept, language);
        extracted.normalization = code_normalization();
        if (!language.empty()) {
            extracted.language = language;
        }
        if (!content.suggested_title.empty()) {
            extracted.title = content.suggested_title;
        }

        if (mixes_indentation(lines)) {
            extracted.warnings.emplace_back(
                    "this file indents with both tabs and spaces, which look identical on screen — "
                    "expect to guess at which one each line wants");
        }
        const auto longest = std::ranges::max_element(lines, {}, &std::string_view::size);
        if (longest != lines.end() && longest->size() > options_.long_line) {
            extracted.warnings.emplace_back("the longest line here is " + std::to_string(longest->size()) +
                                            " characters, which is minified rather than written; "
                                            "it will not read as code at the keyboard");
        }
        return extracted;
    }

}  // namespace typeit::app
