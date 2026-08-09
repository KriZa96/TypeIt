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
    ///
    /// Reports what the extractor and the typing-readiness pass had to say as
    /// well (TX-003, TX-007). A source file that mixes tabs and spaces, or a
    /// chapter with four hundred characters no keyboard can reach, is worth
    /// knowing about before somebody wonders why they cannot type it — and a
    /// command line has nowhere else to put that.
    [[nodiscard]] core::Result<std::string> import_text(app::TextLibraryService& library, const CliOptions& options);

    /// `--import-dir DIR`. Every supported file in the folder, one text apiece.
    ///
    /// Reports rather than fails: one unreadable file in a folder of two
    /// hundred does not cost somebody the other hundred and ninety-nine, so the
    /// summary names what went in and what did not, with the reason beside the
    /// filename (TX-004).
    [[nodiscard]] core::Result<std::string> import_directory(app::TextLibraryService& library,
                                                             const CliOptions& options);

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
