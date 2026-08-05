#include "WindowsConsole.h"

#include <string>
#include <vector>

#ifdef _WIN32
// NOLINTNEXTLINE(llvm-include-order) -- windows.h must come before its own headers
#include <windows.h>
#endif

namespace typeit::tui {

#ifdef _WIN32

    WindowsConsole::WindowsConsole() {
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        const HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (out == INVALID_HANDLE_VALUE || in == INVALID_HANDLE_VALUE) {
            // Redirected to a file or a pipe. Nothing to configure and nothing
            // to restore, which is a normal way to run `--export`.
            problems_.emplace_back("no console attached; output is redirected");
            return;
        }

        // Captured before anything is changed, so the destructor puts back what
        // was really there rather than what this code assumes.
        captured_ = GetConsoleMode(out, &previous_output_mode_) != 0 && GetConsoleMode(in, &previous_input_mode_) != 0;
        previous_output_page_ = GetConsoleOutputCP();
        previous_input_page_ = GetConsoleCP();

        if (!captured_) {
            problems_.emplace_back("the console mode could not be read; it will not be restored");
        }

        // UTF-8 both ways. Without this a `č` is two bytes of mojibake, which
        // is the Windows half of the README's multi-byte complaint.
        if (SetConsoleOutputCP(CP_UTF8) == 0 || SetConsoleCP(CP_UTF8) == 0) {
            problems_.emplace_back("the console refused UTF-8; non-ASCII text may not render");
        }

        // VT processing, which is what makes an ANSI escape mean anything.
        // A console too old for it gets the ASCII glyph set instead of an
        // aborted start.
        if (captured_) {
            const DWORD wanted = previous_output_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            virtual_terminal_ = SetConsoleMode(out, wanted) != 0;
            if (!virtual_terminal_) {
                problems_.emplace_back("this console does not support VT processing; using ASCII glyphs");
            }
        }
    }

    WindowsConsole::~WindowsConsole() {
        if (previous_output_page_ != 0) {
            static_cast<void>(SetConsoleOutputCP(previous_output_page_));
        }
        if (previous_input_page_ != 0) {
            static_cast<void>(SetConsoleCP(previous_input_page_));
        }
        if (!captured_) {
            return;
        }
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        const HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (out != INVALID_HANDLE_VALUE) {
            static_cast<void>(SetConsoleMode(out, previous_output_mode_));
        }
        if (in != INVALID_HANDLE_VALUE) {
            static_cast<void>(SetConsoleMode(in, previous_input_mode_));
        }
    }

#else

    // Everywhere else a terminal is already a terminal. The type still exists
    // so the composition root has no platform test in it — a `#ifdef` at the
    // call site is how a platform bug hides.
    WindowsConsole::WindowsConsole() = default;
    WindowsConsole::~WindowsConsole() = default;

#endif

}  // namespace typeit::tui
