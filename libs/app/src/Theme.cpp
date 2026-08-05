#include "typeit/app/Theme.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace typeit::app {
    namespace {

        /// Key to colour, in one table so the two directions cannot disagree.
        constexpr std::array<std::pair<std::string_view, ThemeColor>, 14> kKeys{{
                {"background", ThemeColor::Background},
                {"surface", ThemeColor::Surface},
                {"border", ThemeColor::Border},
                {"text_pending", ThemeColor::TextPending},
                {"text_correct", ThemeColor::TextCorrect},
                {"text_incorrect", ThemeColor::TextIncorrect},
                {"text_corrected", ThemeColor::TextCorrected},
                {"caret", ThemeColor::Caret},
                {"pacer", ThemeColor::Pacer},
                {"accent", ThemeColor::Accent},
                {"success", ThemeColor::Success},
                {"warning", ThemeColor::Warning},
                {"error", ThemeColor::Error},
                {"muted", ThemeColor::Muted},
        }};

        /// One hex digit, or 16 for anything that is not one.
        constexpr std::uint8_t hex_value(char digit) {
            if (digit >= '0' && digit <= '9') {
                return static_cast<std::uint8_t>(digit - '0');
            }
            if (digit >= 'a' && digit <= 'f') {
                return static_cast<std::uint8_t>(digit - 'a' + 10);
            }
            if (digit >= 'A' && digit <= 'F') {
                return static_cast<std::uint8_t>(digit - 'A' + 10);
            }
            return 16;
        }

    }  // namespace

    std::string_view to_string(ThemeColor color) {
        for (const auto& [key, value]: kKeys) {
            if (value == color) {
                return key;
            }
        }
        // Unreachable: the table covers the enum, and a test walks it to prove
        // so. One line is cheaper than the alternative.
        return "unknown";
    }

    bool theme_color_from(std::string_view key, ThemeColor& out) {
        for (const auto& [name, value]: kKeys) {
            if (name == key) {
                out = value;
                return true;
            }
        }
        return false;
    }

    bool rgb_from_hex(std::string_view text, Rgb& out) {
        if (text.size() != 7 || text.front() != '#') {
            return false;
        }

        std::array<std::uint8_t, 6> digits{};
        for (std::size_t at = 0; at < digits.size(); ++at) {
            digits.at(at) = hex_value(text.at(at + 1));
            if (digits.at(at) > 15) {
                return false;
            }
        }

        out.red = static_cast<std::uint8_t>((digits.at(0) << 4U) | digits.at(1));
        out.green = static_cast<std::uint8_t>((digits.at(2) << 4U) | digits.at(3));
        out.blue = static_cast<std::uint8_t>((digits.at(4) << 4U) | digits.at(5));
        return true;
    }

    std::string to_hex(Rgb color) {
        constexpr std::string_view kDigits = "0123456789abcdef";
        std::string out = "#";
        for (const std::uint8_t channel: {color.red, color.green, color.blue}) {
            out += kDigits.at((channel >> 4U) & 0xFU);
            out += kDigits.at(channel & 0xFU);
        }
        return out;
    }

}  // namespace typeit::app
