// Reading a text from standard input, and getting the keyboard back (TI-111).
//
// `cat article.txt | typeit -` is two problems, and the second is the one that
// bites. Reading the pipe is ordinary. But the process's standard input *is*
// the pipe, so once it has been drained there is no keyboard: FTXUI attaches to
// stdin, finds a pipe at end of file, and the program starts with a screen
// nobody can type into. The terminal has to be reopened from the device before
// the loop begins.
//
// Split into two functions so the reading half is testable without a pipe and
// the reattaching half without a terminal. A single "read stdin and fix it up"
// would be testable with neither.
#ifndef TYPEIT_INFRA_TERM_STANDARDINPUT_H
#define TYPEIT_INFRA_TERM_STANDARDINPUT_H

#include <filesystem>
#include <iosfwd>
#include <string>

#include "typeit/core/util/Result.h"

namespace typeit::infra {

    /// Everything on a stream, read whole.
    ///
    /// Binary is not rejected here: what counts as text is the importer's
    /// question, and it already answers it with a byte offset. A reader that
    /// validated UTF-8 would be a second opinion that could disagree.
    ///
    /// A stream that fails part way through is an error rather than a short
    /// read — half an article silently imported as a whole one is the failure
    /// nobody notices.
    [[nodiscard]] core::Result<std::string> read_stream(std::istream& input);

    /// The controlling terminal, as a path. `/dev/tty` on POSIX, `CONIN$` on
    /// Windows.
    ///
    /// Named rather than inlined so the test can pass a device that exists and
    /// one that does not, and assert what each does — a test that could only
    /// use the real terminal would pass or fail on how CI runs it.
    [[nodiscard]] std::filesystem::path controlling_terminal();

    /// Points `stdin` back at `device`, after a pipe has been drained from it.
    ///
    /// Fails rather than throws: a machine with no controlling terminal — a
    /// service, a build step, a container without a tty — is a normal thing to
    /// be, and the answer there is a clear message rather than a crash.
    [[nodiscard]] core::Status reattach_input(const std::filesystem::path& device);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_TERM_STANDARDINPUT_H
