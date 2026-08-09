#include "typeit/app/ingest/Subtitles.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
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

        [[nodiscard]] std::string_view trimmed(std::string_view line) {
            const std::size_t first = line.find_first_not_of(" \t\r");
            if (first == std::string_view::npos) {
                return {};
            }
            return line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        }

        [[nodiscard]] std::vector<std::string_view> lines_of(std::string_view text) {
            std::vector<std::string_view> lines;
            std::size_t start = 0;
            while (start <= text.size()) {
                const std::size_t end = std::min(text.find('\n', start), text.size());
                lines.push_back(text.substr(start, end - start));
                start = end + 1;
            }
            return lines;
        }

        std::string_view line_at(std::span<const std::string_view> lines, std::size_t at) {
            // Unchecked, exactly as char_at.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return lines[at];
        }

        // ---- recognising the parts of a cue -------------------------------------

        /// The `-->` line, in either dialect.
        ///
        /// Recognised by the arrow and by both sides being timecodes, rather
        /// than by the arrow alone: a line of dialogue can contain `-->`, and a
        /// caption dropped because somebody wrote an arrow in it is a caption
        /// nobody can explain the absence of.
        [[nodiscard]] bool is_timing(std::string_view line) { return trimmed(line).contains("-->"); }

        /// Whether a timing line is one this can read.
        ///
        /// SubRip writes `00:00:01,000`, WebVTT writes `00:00:01.000` and
        /// allows the hour to be dropped. Both are digits, colons and one
        /// separator, which is all this needs to know — the times themselves
        /// are thrown away, because a typing test has no clock.
        [[nodiscard]] bool is_valid_timecode(std::string_view stamp) {
            const std::string_view bare = trimmed(stamp);
            if (bare.size() < 8 || bare.find_first_not_of("0123456789:,.") != std::string_view::npos) {
                return false;
            }
            return std::ranges::count(bare, ':') >= 1 && std::ranges::any_of(bare, [](char letter) {
                       return std::isdigit(static_cast<unsigned char>(letter)) != 0;
                   });
        }

        /// Both halves of `start --> end`, checked.
        [[nodiscard]] bool is_well_formed_timing(std::string_view line) {
            const std::string_view bare = trimmed(line);
            const std::size_t arrow = bare.find("-->");
            if (arrow == std::string_view::npos) {
                return false;
            }
            // Positioning settings ride on the end of a WebVTT timing line:
            // `align:start position:10%`. They are the renderer's business.
            std::string_view end = trimmed(bare.substr(arrow + 3));
            if (const std::size_t space = end.find(' '); space != std::string_view::npos) {
                end = end.substr(0, space);
            }
            return is_valid_timecode(bare.substr(0, arrow)) && is_valid_timecode(end);
        }

        /// `<i>`, `<c.yellow>`, `{\an8}` and the karaoke `<00:00:01.000>`.
        ///
        /// Stripped rather than kept: they are instructions to a renderer, and
        /// typing `<i>` is typing markup that was never on screen.
        [[nodiscard]] std::string without_tags(std::string_view line) {
            std::string out;
            out.reserve(line.size());
            for (std::size_t at = 0; at < line.size();) {
                const char letter = char_at(line, at);
                const char closer = letter == '<' ? '>' : '}';
                if (letter != '<' && letter != '{') {
                    out += letter;
                    ++at;
                    continue;
                }
                const std::size_t close = line.find(closer, at);
                if (close == std::string_view::npos) {
                    // An unclosed bracket is arithmetic or an emoticon, not a
                    // tag that happens to run off the end of the line.
                    out += letter;
                    ++at;
                    continue;
                }
                at = close + 1;
            }
            return out;
        }

        // ---- assembling sentences ------------------------------------------------

        /// Whether the text so far has finished saying something.
        ///
        /// A cue is wrapped to a screen width, so its breaks fall where the
        /// width ran out rather than where the sentence did. Joining on
        /// punctuation puts the breaks back where the speaker put them.
        [[nodiscard]] bool ends_sentence(std::string_view text) {
            std::string_view bare = trimmed(text);
            // A closing quote or bracket after the stop still ends it.
            while (!bare.empty() &&
                   (bare.back() == '"' || bare.back() == '\'' || bare.back() == ')' || bare.back() == ']')) {
                bare.remove_suffix(1);
            }
            if (bare.empty()) {
                return false;
            }
            const char last = bare.back();
            return last == '.' || last == '!' || last == '?' || bare.ends_with("…");
        }

        /// The lines of one cue, and everything else thrown away.
        class Cues {
        public:
            void add(std::string cue) {
                if (trimmed(cue).empty()) {
                    return;
                }
                // Captions repeat a cue verbatim across a scene change more
                // often than one would think, and a typing test that says the
                // same sentence twice in a row reads as a bug in the test.
                if (!cues_.empty() && cues_.back() == cue) {
                    return;
                }
                cues_.push_back(std::move(cue));
            }

            [[nodiscard]] std::string prose() const {
                std::string out;
                std::string sentence;
                for (const std::string& cue: cues_) {
                    if (!sentence.empty()) {
                        sentence += ' ';
                    }
                    sentence += cue;
                    if (ends_sentence(sentence)) {
                        out += sentence;
                        out += '\n';
                        sentence.clear();
                    }
                }
                // Whatever was left when the file ran out. A subtitle track
                // trailing off mid-sentence is common and is still text.
                if (!sentence.empty()) {
                    out += sentence;
                    out += '\n';
                }
                return out;
            }

            [[nodiscard]] bool empty() const { return cues_.empty(); }

        private:
            std::vector<std::string> cues_;
        };

        /// One cue's worth of text lines, joined. Returns the line after it.
        [[nodiscard]] std::size_t take_cue_text(std::span<const std::string_view> lines, std::size_t at, Cues& cues) {
            std::string text;
            for (; at < lines.size(); ++at) {
                const std::string_view line = trimmed(line_at(lines, at));
                if (line.empty()) {
                    break;
                }
                const std::string stripped = without_tags(line);
                const std::string_view spoken = trimmed(stripped);
                if (spoken.empty()) {
                    // A cue that was nothing but a positioning tag.
                    continue;
                }
                if (!text.empty()) {
                    text += ' ';
                }
                text += spoken;
            }
            cues.add(std::move(text));
            return at;
        }

    }  // namespace

    std::span<const std::string_view> SubtitleExtractor::mime_types() const {
        static constexpr std::array<std::string_view, 2> kTypes{"text/x-subrip", "text/vtt"};
        return kTypes;
    }

    core::Result<ExtractedText> SubtitleExtractor::extract(const FetchedContent& content) const {
        const std::vector<std::string_view> lines = lines_of(as_text(content));

        Cues cues;
        std::size_t malformed = 0;
        for (std::size_t at = 0; at < lines.size();) {
            const std::string_view line = line_at(lines, at);
            if (!is_timing(line)) {
                // An index, the WEBVTT header, a NOTE, a cue identifier, or a
                // blank line. None of it was said out loud.
                ++at;
                continue;
            }
            if (!is_well_formed_timing(line)) {
                // Skipped with a count rather than fatal. A subtitle file with
                // one corrupt cue in nine hundred is a file worth importing,
                // and refusing the lot over it would be refusing the film.
                ++malformed;
                ++at;
                continue;
            }
            at = take_cue_text(lines, at + 1, cues);
        }

        ExtractedText extracted;
        extracted.text = cues.prose();
        if (!content.suggested_title.empty()) {
            extracted.title = content.suggested_title;
        }
        if (malformed > 0) {
            extracted.warnings.push_back(std::to_string(malformed) +
                                         (malformed == 1 ? " cue had a timecode this could not read"
                                                         : " cues had timecodes this could not read") +
                                         " and was skipped; the rest imported");
        }
        if (cues.empty()) {
            extracted.warnings.emplace_back("no cues here — this may not be a subtitle file after all");
        }
        return extracted;
    }

}  // namespace typeit::app
