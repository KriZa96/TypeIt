#include "typeit/app/Json.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace typeit::app::json {

    void append_string(std::string& out, std::string_view value) {
        out += '"';
        for (const char byte: value) {
            switch (byte) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(byte) < 0x20) {
                        // The control characters JSON has no short escape for.
                        // Everything above them, UTF-8 included, passes through
                        // as itself.
                        constexpr std::string_view kHex = "0123456789abcdef";
                        out += "\\u00";
                        out += kHex.at((static_cast<unsigned char>(byte) >> 4U) & 0xFU);
                        out += kHex.at(static_cast<unsigned char>(byte) & 0xFU);
                    } else {
                        out += byte;
                    }
                    break;
            }
        }
        out += '"';
    }

    std::string number(double value) {
        // The metrics cannot produce a NaN or an infinity; a field that could
        // would be a field that breaks every consumer of the export.
        if (!(value == value) || value > 1e308 || value < -1e308) {
            return "0";
        }
        std::string text = std::to_string(value);
        // std::to_string gives six decimals; trim the ones that say nothing.
        const std::size_t last = text.find_last_not_of('0');
        if (text.contains('.') && last != std::string::npos) {
            text.erase(text.at(last) == '.' ? last : last + 1);
        }
        return text;
    }

}  // namespace typeit::app::json
