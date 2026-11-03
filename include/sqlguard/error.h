#ifndef SQLGUARD_ERROR_H
#define SQLGUARD_ERROR_H

#include <cstddef>
#include <string>
#include <utility>

#include "sqlguard/export.h"

namespace sqlguard {

    /** Position value used when an error has no meaningful source offset. */
    inline constexpr std::size_t noPosition = static_cast<std::size_t>(-1);

    /** Stable error codes returned through sqlguard::Result. */
    enum class ErrorCode {
        None,
        EmptySql,
        EmbeddedNull,
        UnterminatedLiteral,
        UnterminatedComment,
        TooManyParameters,
        StatementNotAllowed,
        PlaceholderOutOfRange,
        DuplicateBinding,
        BindingCountMismatch,
        UnboundParameter,
        InvalidIdentifier,
        IdentifierTooLong,
        IdentifierNotAllowed
    };

    /** Human-readable name of an error code. */
    SQLGUARD_API const char* toString(ErrorCode code) noexcept;

    /** Error payload with a stable code, optional position and explanation. */
    struct Error {
        /** Error category. */
        ErrorCode code = ErrorCode::None;

        /** Byte offset in the input, or noPosition. */
        std::size_t position = noPosition;

        /** Diagnostic text; never localised. */
        std::string message;

        /** Creates an empty successful error payload. */
        Error() = default;

        /** Creates an error payload. */
        Error(ErrorCode errorCode, std::size_t errorPosition, std::string errorMessage)
            : code(errorCode), position(errorPosition), message(std::move(errorMessage))
        {
        }
    };

} // namespace sqlguard

#endif
