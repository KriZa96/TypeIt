#include "typeit/infra/term/Capabilities.h"

#include <optional>
#include <string>
#include <string_view>

#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::infra {
    namespace {

        /// The spellings `TYPEIT_COLOR` accepts, which are the configuration
        /// file's `appearance.color_depth` values minus `auto` — an override
        /// that says "detect" is not an override.
        std::optional<ColorDepth> color_named(std::string_view name) {
            if (name == "mono") {
                return ColorDepth::Mono;
            }
            if (name == "16") {
                return ColorDepth::Ansi16;
            }
            if (name == "256") {
                return ColorDepth::Ansi256;
            }
            if (name == "truecolor") {
                return ColorDepth::TrueColor;
            }
            return std::nullopt;
        }

        std::optional<GlyphSet> glyphs_named(std::string_view name) {
            if (name == "ascii") {
                return GlyphSet::Ascii;
            }
            if (name == "unicode") {
                return GlyphSet::Unicode;
            }
            return std::nullopt;
        }

        /// A glyph set to go with a colour depth, for the rules that only speak
        /// about colour. Anything that can draw 256 colours can draw a box.
        GlyphSet glyphs_for(ColorDepth depth) {
            return depth == ColorDepth::Ansi256 || depth == ColorDepth::TrueColor ? GlyphSet::Unicode : GlyphSet::Ascii;
        }

    }  // namespace

    Capabilities detect_capabilities(const Environment& environment) {
        // 1. NO_COLOR beats everything, including the explicit overrides below:
        //    somebody who has set it once, globally, should not have to unset it
        //    per program.
        if (environment("NO_COLOR").has_value()) {
            return Capabilities{.color = ColorDepth::Mono, .glyphs = GlyphSet::Ascii, .reason = "NO_COLOR is set"};
        }

        Capabilities detected;
        std::string reason;

        // 2. The explicit overrides, which are what somebody reaches for when
        //    the detection below has guessed wrong about their terminal.
        const std::optional<std::string> color_override = environment("TYPEIT_COLOR");
        const std::optional<std::string> glyph_override = environment("TYPEIT_GLYPHS");

        if (color_override.has_value()) {
            if (const std::optional<ColorDepth> named = color_named(*color_override); named.has_value()) {
                return Capabilities{.color = *named,
                                    .glyphs = glyph_override.has_value()
                                                      ? glyphs_named(*glyph_override).value_or(glyphs_for(*named))
                                                      : glyphs_for(*named),
                                    .reason = "TYPEIT_COLOR=" + *color_override};
            }
        }

        // 3-7. Detection proper, in the documented order.
        const std::optional<std::string> colorterm = environment("COLORTERM");
        const std::optional<std::string> term = environment("TERM");

        if (colorterm.has_value() && (*colorterm == "truecolor" || *colorterm == "24bit")) {
            detected.color = ColorDepth::TrueColor;
            detected.glyphs = GlyphSet::Unicode;
            reason = "COLORTERM=" + *colorterm;
        } else if (environment("WT_SESSION").has_value()) {
            // Windows Terminal, which reports neither COLORTERM nor a useful
            // TERM but does both truecolour and Unicode.
            detected.color = ColorDepth::TrueColor;
            detected.glyphs = GlyphSet::Unicode;
            reason = "WT_SESSION is set (Windows Terminal)";
        } else if (term.has_value() && term->contains("256color")) {
            detected.color = ColorDepth::Ansi256;
            detected.glyphs = GlyphSet::Unicode;
            reason = "TERM=" + *term;
        } else if (term.has_value() && (*term == "linux" || *term == "dumb")) {
            detected.color = ColorDepth::Ansi16;
            detected.glyphs = GlyphSet::Ascii;
            reason = "TERM=" + *term;
        } else {
            // The conservative floor. An unknown terminal gets something that
            // certainly works.
            detected.color = ColorDepth::Ansi16;
            detected.glyphs = GlyphSet::Ascii;
            reason = term.has_value() ? "TERM=" + *term + " is not recognised" : "neither COLORTERM nor TERM is set";
        }

        // A glyph override stands on its own: a terminal with a font that
        // cannot draw box characters is not a terminal that cannot do colour.
        if (glyph_override.has_value()) {
            if (const std::optional<GlyphSet> named = glyphs_named(*glyph_override); named.has_value()) {
                detected.glyphs = *named;
                reason += ", TYPEIT_GLYPHS=" + *glyph_override;
            }
        }

        detected.reason = reason;
        return detected;
    }

    std::string_view to_string(ColorDepth depth) {
        switch (depth) {
            case ColorDepth::Mono:
                return "mono";
            case ColorDepth::Ansi16:
                return "16";
            case ColorDepth::Ansi256:
                return "256";
            case ColorDepth::TrueColor:
                return "truecolor";
        }
        return "16";
    }

    std::string_view to_string(GlyphSet glyphs) {
        switch (glyphs) {
            case GlyphSet::Ascii:
                return "ascii";
            case GlyphSet::Unicode:
                return "unicode";
        }
        return "ascii";
    }

}  // namespace typeit::infra
