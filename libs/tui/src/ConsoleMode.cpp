#include "ConsoleMode.h"

#include <string>
#include <vector>

#ifdef _WIN32
// NOLINTNEXTLINE(llvm-include-order) -- windows.h must come before its own headers
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace typeit::tui {

#ifdef _WIN32

    ConsoleMode::ConsoleMode() {
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

    ConsoleMode::~ConsoleMode() {
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

    ConsoleMode::ConsoleMode() {
        static_assert(sizeof(tcflag_t) <= sizeof(unsigned long),
                      "the header stores c_iflag as an unsigned long to avoid including <termios.h>");

        termios settings{};
        if (tcgetattr(STDIN_FILENO, &settings) != 0) {
            // Not a terminal. Nothing to configure and nothing to restore,
            // which is how `--export` runs into a pipe.
            problems_.emplace_back("no terminal attached; input is redirected");
            return;
        }
        previous_input_flags_ = static_cast<unsigned long>(settings.c_iflag);
        captured_ = true;

        // The whole point. IXON makes the line discipline swallow Ctrl+S and
        // Ctrl+Q as XOFF and XON, so the shipped force-quit key never reaches
        // the program; IXANY would make any key resume a stopped output stream,
        // which is the same trap wearing a hat.
        settings.c_iflag &= static_cast<tcflag_t>(~(IXON | IXANY));
        if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) != 0) {
            problems_.emplace_back("flow control could not be turned off; ctrl-q may not work");
        }
    }

    ConsoleMode::~ConsoleMode() {
        if (!captured_) {
            return;
        }
        // Read back and put one field: FTXUI owns the rest of this structure
        // and restoring a copy taken before it ran would undo its raw mode.
        termios settings{};
        if (tcgetattr(STDIN_FILENO, &settings) != 0) {
            return;
        }
        settings.c_iflag = static_cast<tcflag_t>(previous_input_flags_);
        static_cast<void>(tcsetattr(STDIN_FILENO, TCSANOW, &settings));
    }

#endif

}  // namespace typeit::tui
