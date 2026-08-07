// The text library, from a command line (TI-119).
//
// Importing, listing and removing, as text on stdout. The formatting lives here
// rather than in the composition root for the same reason `Reports.h` does: a
// string is testable and a `std::cout` is not.
//
// **The listing is machine-parseable on purpose.** `typeit --list-texts | awk`
// is how somebody finds the id they want to pass to `--text-id`, so the columns
// are tab-separated, the header is one line, and nothing is aligned with runs
// of spaces that a script would have to guess at.
#ifndef TYPEIT_CLI_TEXTS_H
#define TYPEIT_CLI_TEXTS_H

#include <string>

#include "typeit/app/ports/ITextLibraryRepository.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/cli/Cli.h"
#include "typeit/core/util/Result.h"

namespace typeit::cli {

    /// `--import PATH`. Reports the id, and says plainly when the content was
    /// already there rather than pretending to have done something.
    [[nodiscard]] core::Result<std::string> import_text(app::TextLibraryService& library, const CliOptions& options);

    /// `--list-texts`. One header line, then one line per text: id, words,
    /// difficulty, progress, title. Tab-separated.
    [[nodiscard]] core::Result<std::string> list_texts(const app::ITextLibraryRepository& library,
                                                       const app::TextLibraryService& progress,
                                                       const CliOptions& options);

    /// `--remove-text ID`. Refuses without `--yes` rather than asking: a
    /// prompt on stdin is a prompt a pipe cannot answer, and this is the one
    /// command here that destroys something.
    [[nodiscard]] core::Result<std::string> remove_text(app::ITextLibraryRepository& library,
                                                        const CliOptions& options);

}  // namespace typeit::cli

#endif  // TYPEIT_CLI_TEXTS_H
