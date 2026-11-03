#include "sqlguard/error.h"

namespace sqlguard {

    const char* toString(ErrorCode code) noexcept
    {
        switch(code) {
            case ErrorCode::None:
                return "none";
            case ErrorCode::EmptySql:
                return "empty_sql";
            case ErrorCode::EmbeddedNull:
                return "embedded_null";
            case ErrorCode::UnterminatedLiteral:
                return "unterminated_literal";
            case ErrorCode::UnterminatedComment:
                return "unterminated_comment";
            case ErrorCode::TooManyParameters:
                return "too_many_parameters";
            case ErrorCode::StatementNotAllowed:
                return "statement_not_allowed";
            case ErrorCode::PlaceholderOutOfRange:
                return "placeholder_out_of_range";
            case ErrorCode::DuplicateBinding:
                return "duplicate_binding";
            case ErrorCode::BindingCountMismatch:
                return "binding_count_mismatch";
            case ErrorCode::UnboundParameter:
                return "unbound_parameter";
            case ErrorCode::InvalidIdentifier:
                return "invalid_identifier";
            case ErrorCode::IdentifierTooLong:
                return "identifier_too_long";
            case ErrorCode::IdentifierNotAllowed:
                return "identifier_not_allowed";
        }
        return "unknown";
    }

} // namespace sqlguard
