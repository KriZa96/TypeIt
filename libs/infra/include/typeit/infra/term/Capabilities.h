// Detecting what this terminal can do (UX §6.2).
//
// Reading `COLORTERM` is I/O, which is why this lives in `infra`; the answer
// it produces is `app::Capabilities`, which lives one layer up so that `tui`
// can render against it without seeing `infra`.
//
// The floor is deliberately conservative: an unknown terminal gets something
// that certainly works rather than something that probably looks better. A box
// drawing character that renders as a question mark is worse than a `+`.
//
// 1.0 decided this at *compile* time with `FTXUI_MICROSOFT_TERMINAL_FALLBACK`,
// a macro no build file ever defines — so the Unicode branch always compiled
// and the fallback never once ran.
#ifndef TYPEIT_INFRA_TERM_CAPABILITIES_H
#define TYPEIT_INFRA_TERM_CAPABILITIES_H

#include "typeit/app/Capabilities.h"
#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::infra {

    /// The precedence order of UX §6.2, top to bottom. The environment is a
    /// parameter rather than a global read, so a test can ask about a terminal
    /// it is not running in.
    ///
    /// A variable that is set but empty counts as unset, which is the rule the
    /// whole `Environment` port follows — and what no-color.org itself says
    /// about `NO_COLOR`, which takes effect "when present and not an empty
    /// string".
    [[nodiscard]] app::Capabilities detect_capabilities(const Environment& environment);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_TERM_CAPABILITIES_H
