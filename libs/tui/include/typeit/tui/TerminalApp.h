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
#include "typeit/app/ports/IHistoryRepository.h"
#include "typeit/app/ports/ITextLibraryRepository.h"
#include "typeit/app/services/HistoryService.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/app/services/TextLibraryService.h"
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

    /// Writes one. The counterpart of `TextLoader`, and a parameter for the
    /// same reason: this is the only way anything in `tui` puts a file on a
    /// disk, and it does it through something the composition root supplied.
    using TextWriter = std::function<core::Status(const std::filesystem::path&, const std::string&)>;

    /// Everything the screens that show recorded history read.
    ///
    /// Two pointers rather than one because `HistoryService` deliberately does
    /// not wrap the plain queries (TI-069) — a method that only forwards is a
    /// method that only forwards — so a screen asks the repository for rows and
    /// the service for the questions that need arithmetic over them.
    ///
    /// All null is a normal state: a layout test should not have to stand up a
    /// database, and every screen that reads history draws an empty state
    /// anyway. That state is the one a new user sees, so it is worth having.
    struct HistorySource {
        const app::HistoryService* service = nullptr;
        const app::IHistoryRepository* records = nullptr;
        /// For "today", which a streak is counted back from. A port rather
        /// than a call, because a streak that changes at midnight is a thing a
        /// test has to be able to stand either side of.
        const core::IWallClock* wall_clock = nullptr;
        /// Minutes east of UTC. Days are local days — a run at 23:30 and one at
        /// 00:30 are two days to the person who did them — and `app` may not
        /// ask the operating system, so the composition root passes the real
        /// offset and a test passes whichever one it is asking about.
        app::UtcOffsetMinutes utc_offset = 0;
    };

    /// What the text library screen reads and writes.
    ///
    /// Two again, and for the same reason as `HistorySource`: the repository
    /// answers the plain questions — list, tag, remove — and the service owns
    /// the ones with an opinion, which is importing and the bookmark
    /// arithmetic. Null is a normal state: a layout test should not have to
    /// stand up a database, and the screen has a first-run state anyway.
    struct LibrarySource {
        app::TextLibraryService* service = nullptr;
        app::ITextLibraryRepository* records = nullptr;
    };

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
        /// Absent means exporting is not offered. A build with nowhere to write
        /// should not show a key that does nothing.
        TextWriter save_text;
        HistorySource history;
        LibrarySource library;

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
