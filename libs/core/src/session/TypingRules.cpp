#include "typeit/core/session/TypingRules.h"

#include <optional>
#include <string_view>

namespace typeit::core {

    std::string_view to_string(StopOnError value) {
        switch (value) {
            case StopOnError::Off:
                return "off";
            case StopOnError::Letter:
                return "letter";
            case StopOnError::Word:
                return "word";
        }
        // Unreachable: every enumerator returns above.
        return "off";
    }

    std::optional<StopOnError> stop_on_error_from(std::string_view name) {
        if (name == "off") {
            return StopOnError::Off;
        }
        if (name == "letter") {
            return StopOnError::Letter;
        }
        if (name == "word") {
            return StopOnError::Word;
        }
        return std::nullopt;
    }

    std::string_view to_string(ConfidenceMode value) {
        switch (value) {
            case ConfidenceMode::Off:
                return "off";
            case ConfidenceMode::On:
                return "on";
            case ConfidenceMode::Max:
                return "max";
        }
        // Unreachable: every enumerator returns above.
        return "off";
    }

    std::optional<ConfidenceMode> confidence_mode_from(std::string_view name) {
        if (name == "off") {
            return ConfidenceMode::Off;
        }
        if (name == "on") {
            return ConfidenceMode::On;
        }
        if (name == "max") {
            return ConfidenceMode::Max;
        }
        return std::nullopt;
    }

}  // namespace typeit::core
