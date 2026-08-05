// Making a Windows console behave like a terminal (TI-096, UX §6.3).
//
// Three things Windows needs and Unix does not: virtual-terminal processing
// turned on, both code pages set to UTF-8, and — the part that is actually
// hard — all of it put back afterwards. A program that leaves a console in
// code page 65001 has broken every batch file the user runs next.
//
// RAII, so restoration happens on a normal return, on an exception unwinding
// through `main`, and on anything else that runs destructors. Ctrl+C does not
// run destructors, so it is handled separately by asking Windows not to kill
// the process before FTXUI has seen the key.
//
// On every other platform this is an empty object. It is not `#ifdef`-ed at
// the call site, because a composition root littered with platform tests is
// how platform bugs hide.
#ifndef TYPEIT_TUI_WINDOWSCONSOLE_H
#define TYPEIT_TUI_WINDOWSCONSOLE_H

#include <string>
#include <vector>

namespace typeit::tui {

    class WindowsConsole {
    public:
        /// Captures the current state and applies ours. Never throws and never
        /// fails hard: a console too old for VT processing gets the ASCII
        /// glyph set and a note in `--doctor`, not an aborted start.
        WindowsConsole();

        /// Puts back exactly what was captured.
        ~WindowsConsole();

        WindowsConsole(const WindowsConsole&) = delete;
        WindowsConsole& operator=(const WindowsConsole&) = delete;
        WindowsConsole(WindowsConsole&&) = delete;
        WindowsConsole& operator=(WindowsConsole&&) = delete;

        /// Whether virtual-terminal processing is on. False on a console that
        /// refused it, and on every non-Windows platform, where the question
        /// does not arise.
        [[nodiscard]] bool virtual_terminal() const noexcept { return virtual_terminal_; }

        /// What went wrong, for `--doctor`. Empty when everything worked, and
        /// empty on other platforms.
        [[nodiscard]] const std::vector<std::string>& problems() const noexcept { return problems_; }

    private:
        bool virtual_terminal_ = false;
        std::vector<std::string> problems_;

#ifdef _WIN32
        /// The state to restore. Untyped, so the header names no Windows type
        /// — the composition root includes this, and `windows.h` in a public
        /// header is how a build starts defining `min` and `max`. Guarded
        /// because a member nothing reads is a warning everywhere else.
        unsigned long previous_output_mode_ = 0;
        unsigned long previous_input_mode_ = 0;
        unsigned int previous_output_page_ = 0;
        unsigned int previous_input_page_ = 0;
        bool captured_ = false;
#endif
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_WINDOWSCONSOLE_H
