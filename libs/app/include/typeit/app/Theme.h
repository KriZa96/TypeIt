// A theme, as data (UX §4).
//
// Colours are **semantic, never positional**: `text_incorrect`, not `color_3`.
// A theme author never has to know where a colour is used, and adding a UI
// element does not invalidate every theme ever written.
//
// It lives in `app` rather than in `tui`, where ARCHITECTURE §4.4's directory
// sketch puts it, for the same reason `Capabilities` ended up in `infra`: two
// layers need it and only one of them may see the other. `infra` parses the
// TOML — `tui` may not link toml++ — and `tui` renders it, so the value type
// has to sit where both can reach. It names nothing from FTXUI and nothing
// from toml++; it is fourteen colours and a name.
//
// Authored in truecolor, always. `tui::ColorQuantizer` maps down to 256, to 16
// and to no colour at all, so an author writes one file and it works
// everywhere — as opposed to 1.0, which hardcodes 256-palette entries with no
// fallback whatever.
#ifndef TYPEIT_APP_THEME_H
#define TYPEIT_APP_THEME_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace typeit::app {

    /// Eight bits a channel, which is what a theme file writes and what a
    /// truecolor terminal takes.
    struct Rgb {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;

        friend constexpr bool operator==(const Rgb&, const Rgb&) = default;
    };

    /// Every colour a theme names. The enum exists so that "every theme
    /// defines every colour" can be a loop in a test rather than fourteen
    /// copied assertions, and so that adding one is a compile error in the
    /// places that must be updated.
    enum class ThemeColor : std::uint8_t {
        Background,
        Surface,
        Border,
        TextPending,
        TextCorrect,
        TextIncorrect,
        TextCorrected,
        Caret,
        Pacer,
        Accent,
        Success,
        Warning,
        Error,
        Muted,
    };

    /// Every enumerator, in declaration order — the same trick
    /// `core::kAllErrorCodes` uses, and for the same reason: a table-driven
    /// test can cover the enum without a sentinel every switch would have to
    /// handle.
    inline constexpr std::array<ThemeColor, 14> kAllThemeColors{
            ThemeColor::Background,  ThemeColor::Surface,       ThemeColor::Border,        ThemeColor::TextPending,
            ThemeColor::TextCorrect, ThemeColor::TextIncorrect, ThemeColor::TextCorrected, ThemeColor::Caret,
            ThemeColor::Pacer,       ThemeColor::Accent,        ThemeColor::Success,       ThemeColor::Warning,
            ThemeColor::Error,       ThemeColor::Muted,
    };

    /// The key a theme file writes for each colour. One spelling, so the
    /// loader and any future settings screen cannot disagree about it.
    [[nodiscard]] std::string_view to_string(ThemeColor color);

    /// The colour a key names, or nothing. Unknown keys are ignored rather
    /// than refused — a theme written for a later version should still load
    /// on this one (VERSIONING §5).
    [[nodiscard]] bool theme_color_from(std::string_view key, ThemeColor& out);

    /// The theme format this binary understands. Bumped on a breaking change;
    /// unknown keys are ignored, so most theme changes need no bump.
    inline constexpr int kThemeVersion = 1;

    struct Theme {
        std::string name = "typeit-dark";
        std::string author;
        std::string description;

        /// Indexed by `ThemeColor`. Defaulted to `typeit-dark`, which is what
        /// makes "a missing colour key takes the default" and "a missing theme
        /// falls back to the default" the same code path rather than two.
        std::array<Rgb, 14> colors{{
                Rgb{.red = 0x1e, .green = 0x1e, .blue = 0x2e},  // background
                Rgb{.red = 0x31, .green = 0x32, .blue = 0x44},  // surface
                Rgb{.red = 0x45, .green = 0x47, .blue = 0x5a},  // border
                Rgb{.red = 0x6c, .green = 0x70, .blue = 0x86},  // text_pending
                Rgb{.red = 0xcd, .green = 0xd6, .blue = 0xf4},  // text_correct
                Rgb{.red = 0xf3, .green = 0x8b, .blue = 0xa8},  // text_incorrect
                Rgb{.red = 0xf9, .green = 0xe2, .blue = 0xaf},  // text_corrected
                Rgb{.red = 0x89, .green = 0xb4, .blue = 0xfa},  // caret
                Rgb{.red = 0xa6, .green = 0xe3, .blue = 0xa1},  // pacer
                Rgb{.red = 0x89, .green = 0xb4, .blue = 0xfa},  // accent
                Rgb{.red = 0xa6, .green = 0xe3, .blue = 0xa1},  // success
                Rgb{.red = 0xf9, .green = 0xe2, .blue = 0xaf},  // warning
                Rgb{.red = 0xf3, .green = 0x8b, .blue = 0xa8},  // error
                Rgb{.red = 0x58, .green = 0x5b, .blue = 0x70},  // muted
        }};

        [[nodiscard]] Rgb color(ThemeColor which) const { return colors.at(static_cast<std::size_t>(which)); }

        void set(ThemeColor which, Rgb value) { colors.at(static_cast<std::size_t>(which)) = value; }
    };

    /// `#rrggbb`, the only form a theme file may write. `#rgb` is deliberately
    /// not accepted: it is a second spelling of the same thing, and a loader
    /// that takes both invites files that only work on one of them.
    [[nodiscard]] bool rgb_from_hex(std::string_view text, Rgb& out);

    /// `#rrggbb`, lower case. The round trip of the above.
    [[nodiscard]] std::string to_hex(Rgb color);

}  // namespace typeit::app

#endif  // TYPEIT_APP_THEME_H
