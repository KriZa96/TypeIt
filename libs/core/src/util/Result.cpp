#include "typeit/core/util/Result.h"

#include <string>
#include <string_view>
#include <utility>

namespace typeit::core {

    std::string_view default_message(ErrorCode code) noexcept {
        // No default case: -Wswitch then makes a code without a message a build
        // failure, which is the only way this table stays complete.
        switch (code) {
            case ErrorCode::FileNotFound:
                return "That file does not exist.";
            case ErrorCode::FileUnreadable:
                return "That file could not be read.";
            case ErrorCode::InvalidUtf8:
                return "That text is not valid UTF-8.";
            case ErrorCode::EmptyText:
                return "That text is empty.";
            case ErrorCode::TextTooLarge:
                return "That text is too large to load.";
            case ErrorCode::ConfigParse:
                return "The configuration file could not be parsed.";
            case ErrorCode::ConfigInvalid:
                return "The configuration contains a value that is not allowed.";
            case ErrorCode::DbOpen:
                return "The history database could not be opened.";
            case ErrorCode::DbMigrate:
                return "The history database could not be migrated to this version.";
            case ErrorCode::DbQuery:
                return "The history database rejected a query.";
            case ErrorCode::UnknownTheme:
                return "No theme by that name is installed.";
            case ErrorCode::UnknownMode:
                return "No game mode by that name is registered.";
            case ErrorCode::InvalidKeyBinding:
                return "That key binding could not be understood.";
            case ErrorCode::UnsupportedTerminal:
                return "This terminal cannot run TypeIt.";
            case ErrorCode::InvalidArgument:
                return "That command line could not be understood.";
            case ErrorCode::InvalidScript:
                return "That keystroke script could not be understood.";
        }
        // Only reachable through a cast from an out-of-range integer.
        return "An unknown error occurred.";
    }

    Error make_error(ErrorCode code, std::string context) {
        return Error{.code = code, .message = std::string{default_message(code)}, .context = std::move(context)};
    }

    std::string to_string(const Error& error) {
        if (error.context.empty()) {
            return error.message;
        }
        return error.message + " (" + error.context + ")";
    }

}  // namespace typeit::core
