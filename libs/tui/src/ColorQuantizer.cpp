#include "ColorQuantizer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ftxui/screen/color.hpp>

#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"

namespace typeit::tui {
    namespace {

        /// The six levels of each channel in the xterm-256 colour cube. Not
        /// evenly spaced — the gap from 0 to 95 is the largest — which is why
        /// this is a table rather than arithmetic.
        constexpr std::array<int, 6> kCubeLevels{0, 95, 135, 175, 215, 255};

        /// A colour with less spread than this between its channels is a
        /// grey, whatever its hue arithmetic says. Pastels sit just above it,
        /// which is the whole reason this threshold is a named constant rather
        /// than a guess inside an `if`.
        constexpr int kGreyThreshold = 40;

        /// Above this, the brightest channel makes the colour a *bright* ANSI
        /// one. Below, the dim variant. 1.0 had no such distinction because it
        /// had no fallback at all.
        constexpr int kBrightThreshold = 200;

        int channel_to_cube(int value) {
            std::size_t best = 0;
            int best_distance = 256;
            for (std::size_t level = 0; level < kCubeLevels.size(); ++level) {
                const int distance = std::abs(value - kCubeLevels.at(level));
                if (distance < best_distance) {
                    best_distance = distance;
                    best = level;
                }
            }
            return static_cast<int>(best);
        }

        /// Which sixth of the colour wheel a colour falls in, as the ANSI
        /// index of that hue. The usual max-channel formulation, in integers.
        int ansi_hue(app::Rgb color) {
            const int red = color.red;
            const int green = color.green;
            const int blue = color.blue;
            const int highest = std::max({red, green, blue});
            const int lowest = std::min({red, green, blue});
            const int spread = highest - lowest;

            // Degrees, scaled by 60 to stay in integers: 0 red, 1 yellow,
            // 2 green, 3 cyan, 4 blue, 5 magenta.
            int sixth = 0;
            if (highest == red) {
                sixth = ((((green - blue) * 60) / spread) + 360) % 360;
            } else if (highest == green) {
                sixth = (((blue - red) * 60) / spread) + 120;
            } else {
                sixth = (((red - green) * 60) / spread) + 240;
            }

            // The ANSI order is red, green, yellow, blue, magenta, cyan — not
            // the wheel's order, which is why this is a table and not
            // arithmetic.
            if (sixth < 30 || sixth >= 330) {
                return 1;  // red
            }
            if (sixth < 90) {
                return 3;  // yellow
            }
            if (sixth < 150) {
                return 2;  // green
            }
            if (sixth < 210) {
                return 6;  // cyan
            }
            if (sixth < 270) {
                return 4;  // blue
            }
            return 5;  // magenta
        }

    }  // namespace

    std::uint8_t luminance(app::Rgb color) {
        // Rec. 601, in integers: the weighting a terminal's own dimming uses,
        // and close enough for deciding which of sixteen colours is darker.
        const int weighted = (299 * static_cast<int>(color.red)) + (587 * static_cast<int>(color.green)) +
                             (114 * static_cast<int>(color.blue));
        return static_cast<std::uint8_t>(weighted / 1000);
    }

    std::uint8_t xterm256_index(app::Rgb color) {
        // The grey ramp first: a colour whose channels are within a few steps
        // of each other lands far better on the 24 greys than on the cube,
        // where the nearest entry can be visibly tinted.
        const int highest = std::max({color.red, color.green, color.blue});
        const int lowest = std::min({color.red, color.green, color.blue});
        if (highest - lowest <= 8) {
            const int level = luminance(color);
            if (level < 8) {
                return 16;  // The cube's black, which is truer than grey 0.
            }
            if (level > 238) {
                return 231;  // And its white.
            }
            // 24 greys from 8 to 238, evenly spaced this time.
            const int step = std::clamp((level - 8) * 24 / 231, 0, 23);
            return static_cast<std::uint8_t>(232 + step);
        }

        const int red = channel_to_cube(color.red);
        const int green = channel_to_cube(color.green);
        const int blue = channel_to_cube(color.blue);
        return static_cast<std::uint8_t>(16 + (36 * red) + (6 * green) + blue);
    }

    std::uint8_t ansi16_index(app::Rgb color) {
        // Hue and lightness rather than nearest RGB. Nearest-RGB looks
        // reasonable and is wrong for exactly the palettes people write: a
        // pastel pink is numerically closer to white than to red, so a whole
        // theme of pastels collapses into two shades of white and every typing
        // state looks the same. Ask what colour it *is*, then how bright.
        const int highest = std::max({color.red, color.green, color.blue});
        const int lowest = std::min({color.red, color.green, color.blue});

        if (highest - lowest < kGreyThreshold) {
            // A grey. Four of the sixteen are greys, and luminance picks one.
            const int level = luminance(color);
            if (level < 48) {
                return 0;  // black
            }
            if (level < 128) {
                return 8;  // bright black, which every terminal draws as grey
            }
            if (level < kBrightThreshold) {
                return 7;  // white, which is really light grey
            }
            return 15;  // bright white
        }

        const int hue = ansi_hue(color);
        return static_cast<std::uint8_t>(highest >= kBrightThreshold ? hue + 8 : hue);
    }

    ftxui::Color quantize(app::Rgb color, app::ColorDepth depth) {
        switch (depth) {
            case app::ColorDepth::TrueColor:
                return ftxui::Color::RGB(color.red, color.green, color.blue);
            case app::ColorDepth::Ansi256:
                return ftxui::Color{static_cast<ftxui::Color::Palette256>(xterm256_index(color))};
            case app::ColorDepth::Ansi16:
                return ftxui::Color{static_cast<ftxui::Color::Palette16>(ansi16_index(color))};
            case app::ColorDepth::Mono:
                // Nothing left to say with colour. `style_for` says it with
                // attributes instead.
                return ftxui::Color::Default;
        }
        return ftxui::Color::Default;
    }

    Styling style_for(const app::Theme& theme, app::ThemeColor which, app::ColorDepth depth) {
        Styling styling;
        styling.color = quantize(theme.color(which), depth);
        if (depth != app::ColorDepth::Mono) {
            return styling;
        }

        // The four typing states have to be told apart with no colour at all,
        // and underline, reverse and bold are the only three ways left. The
        // rest are drawn plain — a monochrome terminal that underlined
        // everything would be worse than one that underlined nothing.
        switch (which) {
            case app::ThemeColor::TextIncorrect:
                styling.underline = true;
                return styling;
            case app::ThemeColor::TextCorrected:
                styling.underline = true;
                styling.bold = true;
                return styling;
            case app::ThemeColor::TextCorrect:
                styling.bold = true;
                return styling;
            case app::ThemeColor::Caret:
            case app::ThemeColor::Pacer:
                styling.inverted = true;
                return styling;
            case app::ThemeColor::TextPending:
            case app::ThemeColor::Background:
            case app::ThemeColor::Surface:
            case app::ThemeColor::Border:
            case app::ThemeColor::Accent:
            case app::ThemeColor::Success:
            case app::ThemeColor::Warning:
            case app::ThemeColor::Error:
            case app::ThemeColor::Muted:
                return styling;
        }
        return styling;
    }

}  // namespace typeit::tui
