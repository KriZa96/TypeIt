// The terminal, owned in one place (ARCHITECTURE §4.4, TI-079).
//
// Everything FTXUI is behind this. The library links it `PRIVATE` and this
// header names none of it — not a type, not an include — so a consumer of
// `typeit::tui` gets the event loop without inheriting a rendering library it
// has no business knowing about. That is what makes the composition root the
// only translation unit where FTXUI and SQLite are both visible.
//
// 1.0 spread the terminal across three classes and a detached thread: `Screen`
// owned a `ScreenInteractive` and a polling thread, `Main` drove the loop, and
// `GameState` carried five booleans that any of them could flip. The
// replacement owns the screen here, and navigation becomes an explicit stack
// (TI-081) rather than a set of flags.
//
// **Single-threaded by contract** (ARCHITECTURE §6.4). Everything that mutates
// state happens on the loop's thread; the frame ticker of TI-080 posts an
// event and touches nothing.
#ifndef TYPEIT_TUI_TERMINALAPP_H
#define TYPEIT_TUI_TERMINALAPP_H

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/IClock.h"
#include "typeit/core/util/Result.h"

namespace typeit::tui {

    /// A text the menu can offer, already located. Resolving where the bundled
    /// corpora live is infra's job (defect C2 was 1.0 doing it with `__FILE__`
    /// at runtime), and `tui` may not see infra — so the composition root hands
    /// the paths over already found.
    struct TextChoice {
        std::string name;
        std::filesystem::path path;
    };

    /// Reads one. A parameter rather than a call, for the same reason: this is
    /// the only way anything in `tui` reaches a disk, and it reaches it through
    /// something the composition root supplied.
    using TextLoader = std::function<core::Result<std::string>(const std::filesystem::path&)>;

    /// Everything the application needs from below it. All borrowed; the
    /// composition root owns every one of them and outlives the application.
    struct Dependencies {
        const app::SessionService* sessions = nullptr;
        const core::Config* config = nullptr;
        const app::Theme* theme = nullptr;
        const core::IClock* clock = nullptr;
        app::Capabilities capabilities;

        /// What the menu offers, in menu order. Empty is not an error: nothing
        /// found means the run types `text` below, and a user with their own
        /// file loses nothing.
        std::vector<TextChoice> texts;
        TextLoader load_text;

        /// The text a run types when the catalogue is empty. Phase 6 replaces
        /// this with the library; until then the composition root supplies one.
        std::string text;
    };

    class TerminalApp {
    public:
        /// Builds the screen stack and starts on the menu.
        explicit TerminalApp(Dependencies dependencies);

        /// Tears the screen down and restores the terminal. Defined in the
        /// translation unit that knows what it is destroying, which is what
        /// keeps the FTXUI type out of this header.
        ~TerminalApp();

        TerminalApp(const TerminalApp&) = delete;
        TerminalApp& operator=(const TerminalApp&) = delete;
        TerminalApp(TerminalApp&&) = delete;
        TerminalApp& operator=(TerminalApp&&) = delete;

        /// Runs until something calls `quit()`.
        ///
        /// A `quit()` that arrived before this was called is honoured without
        /// entering the loop at all — which is not a special case for tests so
        /// much as the only sane answer to "stop" arriving first.
        void run();

        /// Asks the loop to end. Safe to call before `run()`, during it, and
        /// after it; the second call does nothing the first did not.
        void quit();

        /// Whether `quit()` has been called. Exists so a caller can tell a
        /// loop that ended from one that never started.
        [[nodiscard]] bool is_quitting() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_TERMINALAPP_H
