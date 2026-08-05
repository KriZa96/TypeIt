// What this terminal can actually do (UX §6.2).
//
// Detected at runtime from the environment, which is why it lives in `infra`:
// reading `COLORTERM` is I/O, and the rest of the program is handed the answer
// rather than the question. 1.0 decided this at *compile* time with
// `FTXUI_MICROSOFT_TERMINAL_FALLBACK`, a macro no build file ever defines — so
// the Unicode branch always compiled and the fallback never once ran.
//
// The floor is deliberately conservative: an unknown terminal gets something
// that certainly works rather than something that probably looks better. A box
// drawing character that renders as a question mark is worse than a `+`.
//
// Every conclusion carries the rule that reached it. `--doctor` prints it, and
// "why is this terminal monochrome" is otherwise a question nobody can answer
// remotely.
#ifndef TYPEIT_INFRA_TERM_CAPABILITIES_H
#define TYPEIT_INFRA_TERM_CAPABILITIES_H

#include <cstdint>
#include <string>
#include <string_view>

#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::infra {

    enum class ColorDepth : std::uint8_t {
        Mono,
        Ansi16,
        Ansi256,
        TrueColor,
    };

    enum class GlyphSet : std::uint8_t {
        Ascii,
        Unicode,
    };

    struct Capabilities {
        ColorDepth color = ColorDepth::Ansi16;
        GlyphSet glyphs = GlyphSet::Ascii;
        /// The rule that decided, named — `NO_COLOR is set`,
        /// `COLORTERM=truecolor`. Never empty.
        std::string reason;
    };

    /// The precedence order of UX §6.2, top to bottom. The environment is a
    /// parameter rather than a global read, so a test can ask about a terminal
    /// it is not running in.
    ///
    /// A variable that is set but empty counts as unset, which is the rule the
    /// whole `Environment` port follows — and what no-color.org itself says
    /// about `NO_COLOR`, which takes effect "when present and not an empty
    /// string".
    [[nodiscard]] Capabilities detect_capabilities(const Environment& environment);

    /// The spellings the configuration file and `--doctor` use, so a value and
    /// its name cannot drift apart.
    [[nodiscard]] std::string_view to_string(ColorDepth depth);
    [[nodiscard]] std::string_view to_string(GlyphSet glyphs);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_TERM_CAPABILITIES_H
