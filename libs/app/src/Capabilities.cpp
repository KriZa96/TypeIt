#include "typeit/app/Capabilities.h"

#include <string_view>

namespace typeit::app {

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

}  // namespace typeit::app
