#include "typeit/cli/Texts.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/Json.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/util/Result.h"

namespace typeit::cli {
    namespace {

        /// A field with no tabs or newlines in it, so one text is always one
        /// line and one column is always one field. A title can contain
        /// anything; a listing somebody pipes into `cut` cannot.
        [[nodiscard]] std::string one_line(std::string_view text) {
            std::string out;
            out.reserve(text.size());
            for (const char letter: text) {
                out += (letter == '\t' || letter == '\n' || letter == '\r') ? ' ' : letter;
            }
            return out;
        }

        [[nodiscard]] std::string percent(double fraction) {
            return std::to_string(static_cast<int>(std::lround(fraction * 100.0))) + "%";
        }

    }  // namespace

    namespace {

        /// What the pipeline thought worth mentioning, indented under the text
        /// it is about.
        ///
        /// Extractor warnings first — a file that mixes tabs and spaces is
        /// effectively untypeable and the typist has no way to see why — then
        /// what the readiness pass did and what it could not fix. Silence is
        /// the common case and prints nothing.
        [[nodiscard]] std::string notes_on(const app::ImportOutcome& imported) {
            std::string out;
            for (const std::string& warning: imported.warnings) {
                out += "  " + warning + "\n";
            }
            for (const std::string& line: imported.readiness.lines()) {
                out += "  " + line + "\n";
            }
            return out;
        }

    }  // namespace

    core::Result<std::string> import_text(app::TextLibraryService& library, const CliOptions& options) {
        const core::Result<app::ImportOutcome> imported = library.import_file(options.operand);
        if (!imported) {
            return std::unexpected{imported.error()};
        }

        // Said plainly rather than reported as a fresh import. Somebody who
        // adds the same article twice should learn that it was already there,
        // not that they now have two.
        const std::string what = imported->already_present ? "already in the library as" : "imported as";
        return options.operand + ": " + what + " text " + std::to_string(imported->id.value) + "\n" +
               notes_on(*imported);
    }

    core::Result<std::string> import_directory(app::TextLibraryService& library, const CliOptions& options) {
        const core::Result<app::DirectoryImport> summary = library.import_directory(options.operand);
        if (!summary) {
            return std::unexpected{summary.error()};
        }

        // Counts first, because that is the line somebody reads; the detail
        // after it, because a count of failures says something went wrong
        // without saying what.
        std::string out = options.operand + ": imported " + std::to_string(summary->imported.size()) +
                          (summary->imported.size() == 1 ? " text" : " texts");
        if (!summary->skipped.empty()) {
            out += ", skipped " + std::to_string(summary->skipped.size());
        }
        out += "\n";
        for (const app::ImportOutcome& imported: summary->imported) {
            out += "  text " + std::to_string(imported.id.value) +
                   (imported.already_present ? " (already in the library)" : "") + "\n";
            out += notes_on(imported);
        }
        for (const std::string& skipped: summary->skipped) {
            out += "  skipped " + skipped + "\n";
        }
        return out;
    }

    core::Result<std::string> list_texts(const app::ITextLibraryRepository& library,
                                         const app::TextLibraryService& progress, const CliOptions& options) {
        app::TextFilter filter;
        if (options.last.has_value()) {
            filter.limit = static_cast<std::size_t>(*options.last);
        }

        const core::Result<std::vector<app::TextSummary>> texts = library.list(filter);
        if (!texts) {
            return std::unexpected{texts.error()};
        }
        if (texts->empty()) {
            // A sentence, not a bare header: somebody with an empty library has
            // asked a reasonable question and deserves an answer to it.
            return std::string{"No texts yet. Add one with --import PATH.\n"};
        }

        // Tab-separated, one header line, nothing aligned with runs of spaces
        // that a script would have to guess at. `--list-texts | awk` is how
        // somebody finds the id to pass to `--text-id`.
        std::string out = "id\twords\tdifficulty\tprogress\ttitle\n";
        for (const app::TextSummary& text: *texts) {
            out += std::to_string(text.id.value);
            out += '\t';
            out += std::to_string(text.word_count);
            out += '\t';
            out += text.difficulty.has_value() ? app::json::number(*text.difficulty) : "-";
            out += '\t';

            // A failure to read one text's progress is a dash, not the end of
            // the listing: the other texts are still worth showing.
            const core::Result<app::TextProgress> place = progress.progress(text.id);
            out += place ? percent(place->fraction) : "-";
            out += '\t';
            out += one_line(text.title);
            out += '\n';
        }
        return out;
    }

    core::Result<std::string> remove_text(app::ITextLibraryRepository& library, const CliOptions& options) {
        if (!options.remove_id.has_value()) {
            return core::fail(core::ErrorCode::InvalidArgument, "--remove-text needs the id of a text");
        }
        const core::TextId id = *options.remove_id;

        const core::Result<std::optional<app::TextItem>> existing = library.get(id);
        if (!existing) {
            return std::unexpected{existing.error()};
        }
        // Named rather than reached through two dereferences: clang-tidy
        // cannot see the `has_value` check through the `Result` wrapping the
        // optional, and reports every use of it as unchecked.
        const std::optional<app::TextItem>& text = existing.value();
        if (!text.has_value()) {
            return core::fail(core::ErrorCode::FileNotFound, "no text with id " + std::to_string(id.value));
        }

        if (!options.assume_yes) {
            // Refused rather than prompted. A prompt on stdin is a prompt a
            // pipe cannot answer, and stdin here may well be a piped text —
            // so the confirmation is a flag, and saying which one is the whole
            // message.
            return core::fail(core::ErrorCode::InvalidArgument,
                              "removing \"" + one_line(text->title) +
                                      "\" also removes its tags and bookmark; pass --yes to go ahead");
        }

        if (const core::Status removed = library.remove(id); !removed) {
            return std::unexpected{removed.error()};
        }
        // What actually happened to the rest, because "removed" alone leaves
        // somebody wondering about the runs they typed from it.
        return "removed text " + std::to_string(id.value) + " (\"" + one_line(text->title) +
               "\") and its tags and bookmark; past sessions are kept and no longer name a text\n";
    }

}  // namespace typeit::cli
