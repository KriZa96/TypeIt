#include "typeit/infra/term/Capabilities.h"

#include <optional>
#include <string>
#include <string_view>

#include "typeit/app/Capabilities.h"
#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::infra {
    namespace {

        /// The spellings `TYPEIT_COLOR` accepts, which are the configuration
        /// file's `appearance.color_depth` values minus `auto` — an override
        /// that says "detect" is not an override.
        std::optional<app::ColorDepth> color_named(std::string_view name) {
            if (name == "mono") {
                return app::ColorDepth::Mono;
            }
            if (name == "16") {
                return app::ColorDepth::Ansi16;
            }
            if (name == "256") {
                return app::ColorDepth::Ansi256;
            }
            if (name == "truecolor") {
                return app::ColorDepth::TrueColor;
            }
            return std::nullopt;
        }

        std::optional<app::GlyphSet> glyphs_named(std::string_view name) {
            if (name == "ascii") {
                return app::GlyphSet::Ascii;
            }
            if (name == "unicode") {
                return app::GlyphSet::Unicode;
            }
            return std::nullopt;
        }

        /// A glyph set to go with a colour depth, for the rules that only speak
        /// about colour. Anything that can draw 256 colours can draw a box.
        app::GlyphSet glyphs_for(app::ColorDepth depth) {
            return depth == app::ColorDepth::Ansi256 || depth == app::ColorDepth::TrueColor ? app::GlyphSet::Unicode
                                                                                            : app::GlyphSet::Ascii;
        }

    }  // namespace

    app::Capabilities detect_capabilities(const Environment& environment) {
        // 1. NO_COLOR beats everything, including the explicit overrides below:
        //    somebody who has set it once, globally, should not have to unset it
        //    per program.
        if (environment("NO_COLOR").has_value()) {
            return app::Capabilities{
                    .color = app::ColorDepth::Mono, .glyphs = app::GlyphSet::Ascii, .reason = "NO_COLOR is set"};
        }

        app::Capabilities detected;
        std::string reason;

        // 2. The explicit overrides, which are what somebody reaches for when
        //    the detection below has guessed wrong about their terminal.
        const std::optional<std::string> color_override = environment("TYPEIT_COLOR");
        const std::optional<std::string> glyph_override = environment("TYPEIT_GLYPHS");

        if (color_override.has_value()) {
            if (const std::optional<app::ColorDepth> named = color_named(*color_override); named.has_value()) {
                return app::Capabilities{.color = *named,
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
            detected.color = app::ColorDepth::TrueColor;
            detected.glyphs = app::GlyphSet::Unicode;
            reason = "COLORTERM=" + *colorterm;
        } else if (environment("WT_SESSION").has_value()) {
            // Windows Terminal, which reports neither COLORTERM nor a useful
            // TERM but does both truecolour and Unicode.
            detected.color = app::ColorDepth::TrueColor;
            detected.glyphs = app::GlyphSet::Unicode;
            reason = "WT_SESSION is set (Windows Terminal)";
        } else if (term.has_value() && term->contains("256color")) {
            detected.color = app::ColorDepth::Ansi256;
            detected.glyphs = app::GlyphSet::Unicode;
            reason = "TERM=" + *term;
        } else if (term.has_value() && (*term == "linux" || *term == "dumb")) {
            detected.color = app::ColorDepth::Ansi16;
            detected.glyphs = app::GlyphSet::Ascii;
            reason = "TERM=" + *term;
        } else {
            // The conservative floor. An unknown terminal gets something that
            // certainly works.
            detected.color = app::ColorDepth::Ansi16;
            detected.glyphs = app::GlyphSet::Ascii;
            reason = term.has_value() ? "TERM=" + *term + " is not recognised" : "neither COLORTERM nor TERM is set";
        }

        // A glyph override stands on its own: a terminal with a font that
        // cannot draw box characters is not a terminal that cannot do colour.
        if (glyph_override.has_value()) {
            if (const std::optional<app::GlyphSet> named = glyphs_named(*glyph_override); named.has_value()) {
                detected.glyphs = *named;
                reason += ", TYPEIT_GLYPHS=" + *glyph_override;
            }
        }

        detected.reason = reason;
        return detected;
    }

}  // namespace typeit::infra
