// What a terminal can do, as data (UX §6.2).
//
// The value only. *Detecting* it means reading `COLORTERM`, which is I/O and
// therefore `infra::detect_capabilities` — but `tui` has to render against the
// answer and may not see `infra`, so the answer itself lives here, where both
// can reach it. Exactly the arrangement `app::Theme` has, and for the same
// reason.
#ifndef TYPEIT_APP_CAPABILITIES_H
#define TYPEIT_APP_CAPABILITIES_H

#include <cstdint>
#include <string>
#include <string_view>

namespace typeit::app {

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
        /// `COLORTERM=truecolor`. Never empty once detection has run.
        ///
        /// `--doctor` prints it, and "why is this terminal monochrome" is
        /// otherwise a question nobody can answer remotely.
        std::string reason;
    };

    /// The spellings the configuration file and `--doctor` use, so a value and
    /// its name cannot drift apart.
    [[nodiscard]] std::string_view to_string(ColorDepth depth);
    [[nodiscard]] std::string_view to_string(GlyphSet glyphs);

}  // namespace typeit::app

#endif  // TYPEIT_APP_CAPABILITIES_H
