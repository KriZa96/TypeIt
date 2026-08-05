#include "typeit/cli/Script.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {
    namespace {

        using core::ErrorCode;
        using core::Result;

        constexpr std::string_view kHeaderKeyword = "typeit-script";
        /// The spelling of a space, because trailing whitespace in a fixture is
        /// invisible and the first editor to touch the file would strip it.
        constexpr std::string_view kSpaceKeyword = "space";

        std::unexpected<core::Error> refuse(std::size_t line, const std::string& what) {
            return std::unexpected{
                    core::make_error(ErrorCode::InvalidScript, "line " + std::to_string(line) + ": " + what)};
        }

        std::string_view trimmed(std::string_view text) {
            while (!text.empty() && core::is_ascii_space(text.front())) {
                text.remove_prefix(1);
            }
            while (!text.empty() && core::is_ascii_space(text.back())) {
                text.remove_suffix(1);
            }
            return text;
        }

        /// The first whitespace-delimited word of `text`, and what follows it
        /// with its leading whitespace removed.
        std::pair<std::string_view, std::string_view> split_word(std::string_view text) {
            const std::size_t end = std::min(text.find_first_of(" \t"), text.size());
            return {text.substr(0, end), trimmed(text.substr(end))};
        }

        Result<std::int64_t> as_number(std::string_view text) {
            std::int64_t value = 0;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) -- the pair is the range
            const char* const last = text.data() + text.size();
            // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage) -- `last` is the size
            const std::from_chars_result parsed = std::from_chars(text.data(), last, value);
            if (parsed.ec != std::errc{} || parsed.ptr != last) {
                return std::unexpected{core::make_error(ErrorCode::InvalidScript, std::string{text})};
            }
            return value;
        }

        /// `# typeit-script <N>`, which must be the first line that says
        /// anything.
        Result<int> parse_header(std::size_t line, std::string_view text) {
            const auto [keyword, rest] = split_word(trimmed(text.substr(1)));
            if (keyword != kHeaderKeyword) {
                return refuse(line, "expected the header `# " + std::string{kHeaderKeyword} + " " +
                                            std::to_string(kScriptVersion) + "`");
            }
            const Result<std::int64_t> version = as_number(rest);
            if (!version) {
                return refuse(line, "the header version is not a number: \"" + std::string{rest} + "\"");
            }
            if (*version != kScriptVersion) {
                // Refused rather than half-understood: a format change that
                // silently reinterpreted old scripts would quietly invalidate
                // every fixture in the suite.
                return refuse(line, "script version " + std::to_string(*version) + " (this binary understands " +
                                            std::to_string(kScriptVersion) + ")");
            }
            return static_cast<int>(*version);
        }

        /// The one grapheme a `type` line asks for.
        Result<core::Grapheme> parse_grapheme(std::size_t line, std::string_view operand) {
            const std::string_view spelled = operand == kSpaceKeyword ? std::string_view{" "} : operand;

            const Result<core::TextBuffer> buffer = core::TextBuffer::from_utf8(spelled);
            if (!buffer) {
                return refuse(line, "`type` was given something that is not UTF-8");
            }
            if (buffer->size() != 1) {
                // A line is one keystroke. `type hello` is a mistake, not five
                // keystrokes at the same millisecond, and reading it as five
                // would invent timings nobody wrote.
                return refuse(line, "`type` takes exactly one grapheme, not " + std::to_string(buffer->size()) +
                                            " (a space is spelled `" + std::string{kSpaceKeyword} + "`)");
            }
            return buffer->at(core::GraphemeIndex{0});
        }

        Result<ScriptEvent> parse_event(std::size_t line, std::string_view text, core::Millis previous) {
            const auto [stamp, rest] = split_word(text);
            const Result<std::int64_t> at = as_number(stamp);
            if (!at) {
                return refuse(line, "expected a timestamp in milliseconds, not \"" + std::string{stamp} + "\"");
            }
            if (core::Millis{*at} < previous) {
                // The keystroke log's own precondition. A keyboard cannot
                // travel back in time; a file can, and every window-based
                // metric would be undefined if it did.
                return refuse(line, "the timestamp goes backwards: " + std::to_string(*at) + " after " +
                                            std::to_string(previous.value));
            }

            const auto [verb, operand] = split_word(rest);
            if (verb == "backspace") {
                if (!operand.empty()) {
                    return refuse(line, "`backspace` takes no argument");
                }
                return ScriptEvent{.at = core::Millis{*at}, .typed = {}};
            }
            if (verb != "type") {
                return refuse(line, "unknown verb \"" + std::string{verb} + "\" (expected `type` or `backspace`)");
            }
            if (operand.empty()) {
                return refuse(line, "`type` expects a grapheme");
            }

            const Result<core::Grapheme> typed = parse_grapheme(line, operand);
            if (!typed) {
                return std::unexpected{typed.error()};
            }
            return ScriptEvent{.at = core::Millis{*at}, .typed = *typed};
        }

    }  // namespace

    core::Result<Script> parse_script(std::string_view text) {
        Script script;
        bool header_seen = false;
        core::Millis previous{0};
        std::size_t line = 0;

        while (!text.empty() || line == 0) {
            const std::size_t break_at = text.find('\n');
            const std::string_view raw = text.substr(0, break_at);
            text = break_at == std::string_view::npos ? std::string_view{} : text.substr(break_at + 1);
            ++line;

            const std::string_view content = trimmed(raw);
            if (content.empty()) {
                continue;
            }

            if (content.starts_with('#')) {
                if (header_seen) {
                    continue;  // An ordinary comment.
                }
                const Result<int> version = parse_header(line, content);
                if (!version) {
                    return std::unexpected{version.error()};
                }
                script.version = *version;
                header_seen = true;
                continue;
            }

            if (!header_seen) {
                return refuse(line, "expected the header `# " + std::string{kHeaderKeyword} + " " +
                                            std::to_string(kScriptVersion) + "` first");
            }

            const Result<ScriptEvent> event = parse_event(line, content, previous);
            if (!event) {
                return std::unexpected{event.error()};
            }
            previous = event->at;
            script.events.push_back(*event);
        }

        if (!header_seen) {
            return std::unexpected{core::make_error(ErrorCode::InvalidScript,
                                                    "the script has no `# " + std::string{kHeaderKeyword} + " " +
                                                            std::to_string(kScriptVersion) + "` header")};
        }
        return script;
    }

}  // namespace typeit::cli
