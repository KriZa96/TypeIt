// The one error mechanism that crosses a layer boundary (ADR-009,
// TECHNICAL section 1.2).
//
// Expected failures — a missing file, a malformed config, a database that will
// not open — are values. Exceptions are reserved for broken invariants, which
// are bugs rather than conditions to recover from. Nothing here throws.
#ifndef TYPEIT_CORE_UTIL_RESULT_H
#define TYPEIT_CORE_UTIL_RESULT_H

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace typeit::core {

    enum class ErrorCode {
        FileNotFound,
        FileUnreadable,
        InvalidUtf8,
        EmptyText,
        TextTooLarge,
        ConfigParse,
        ConfigInvalid,
        DbOpen,
        DbMigrate,
        DbQuery,
        UnknownTheme,
        UnknownMode,
        InvalidKeyBinding,
        UnsupportedTerminal,
    };

    /// Every code, in declaration order. Exists so that a table-driven test can
    /// cover the enum without a sentinel enumerator that every switch would then
    /// have to handle.
    inline constexpr std::array kAllErrorCodes{
            ErrorCode::FileNotFound,      ErrorCode::FileUnreadable,
            ErrorCode::InvalidUtf8,       ErrorCode::EmptyText,
            ErrorCode::TextTooLarge,      ErrorCode::ConfigParse,
            ErrorCode::ConfigInvalid,     ErrorCode::DbOpen,
            ErrorCode::DbMigrate,         ErrorCode::DbQuery,
            ErrorCode::UnknownTheme,      ErrorCode::UnknownMode,
            ErrorCode::InvalidKeyBinding, ErrorCode::UnsupportedTerminal,
    };

    struct Error {
        ErrorCode code;
        std::string message;  ///< Human-readable and already formatted.
        std::string context;  ///< A path, a query, a line number. Optional.

        friend bool operator==(const Error&, const Error&) = default;
    };

    /// Whether a bare `std::expected` is [[nodiscard]] is up to the standard
    /// library — gcc 14's libstdc++ and Apple's libc++ do not mark it, newer
    /// libstdc++ does. So the guarantee here is ours, not theirs: **every
    /// function returning Result or Status is marked [[nodiscard]]**, which is
    /// portable and is what tests/core/probes/discards_result.cpp checks.
    template<typename T>
    using Result = std::expected<T, Error>;

    using Status = std::expected<void, Error>;

    /// The default text for a code, used when the caller has nothing more specific
    /// to say. Never empty. Implemented as a switch with no default case, so
    /// adding a code without a message is a compile error rather than a blank
    /// dialog.
    [[nodiscard]] std::string_view default_message(ErrorCode code) noexcept;

    /// An Error carrying the default message for its code.
    [[nodiscard]] Error make_error(ErrorCode code, std::string context = {});

    /// `return fail(ErrorCode::FileNotFound, path);` from any function returning
    /// Result<T> or Status.
    [[nodiscard]] inline std::unexpected<Error> fail(ErrorCode code, std::string context = {}) {
        return std::unexpected{make_error(code, std::move(context))};
    }

    /// For display: the message, with the context appended in parentheses when
    /// there is one.
    [[nodiscard]] std::string to_string(const Error& error);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_UTIL_RESULT_H
