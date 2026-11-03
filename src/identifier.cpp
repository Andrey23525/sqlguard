#include "sqlguard/identifier.h"

#include <algorithm>
#include <utility>

namespace sqlguard {

    namespace detail {

        bool isAsciiLetter(char value) noexcept
        {
            return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        }

        bool isAsciiDigit(char value) noexcept
        {
            return value >= '0' && value <= '9';
        }

        bool isIdentifierStart(char value) noexcept
        {
            return isAsciiLetter(value) || value == '_';
        }

        bool isIdentifierPart(char value) noexcept
        {
            return isIdentifierStart(value) || isAsciiDigit(value);
        }

        Error identifierError(ErrorCode code, std::size_t position, std::string message)
        {
            return Error(code, position, std::move(message));
        }

        Result<void> validateSegment(std::string_view segment, std::size_t offset)
        {
            if(segment.empty()) {
                std::string message = "empty identifier segment";
                return Result<void>::failure(identifierError(ErrorCode::InvalidIdentifier, offset, std::move(message)));
            }

            if(segment.size() > maxIdentifierBytes) {
                std::string message = "identifier segment is too long";
                return Result<void>::failure(identifierError(ErrorCode::IdentifierTooLong, offset, std::move(message)));
            }

            if(!isIdentifierStart(segment.front())) {
                std::string message = "identifier must start with an ASCII letter or underscore";
                return Result<void>::failure(identifierError(ErrorCode::InvalidIdentifier, offset, std::move(message)));
            }

            for(std::size_t index = 1; index < segment.size(); ++index) {
                if(isIdentifierPart(segment[index])) {
                    continue;
                }

                std::string message = "identifier contains a forbidden character";
                std::size_t errorPosition = offset + index;
                return Result<void>::failure(identifierError(ErrorCode::InvalidIdentifier, errorPosition, std::move(message)));
            }

            return Result<void>::success();
        }

        std::string quoteSegment(std::string_view segment, Dialect dialect)
        {
            char openQuote = '"';
            char closeQuote = '"';

            if(dialect == Dialect::MySql) {
                openQuote = '`';
                closeQuote = '`';
            }

            if(dialect == Dialect::SqlServer) {
                openQuote = '[';
                closeQuote = ']';
            }

            std::string quoted;
            quoted.reserve(segment.size() + 2);
            quoted.push_back(openQuote);

            for(const char value : segment) {
                quoted.push_back(value);

                if(value == closeQuote) {
                    quoted.push_back(closeQuote);
                }
            }

            quoted.push_back(closeQuote);
            return quoted;
        }

        std::string quoteIdentifier(std::string_view name, Dialect dialect)
        {
            std::string quoted;
            std::size_t start = 0;
            bool firstPart = true;

            while(start <= name.size()) {
                std::size_t dot = name.find('.', start);
                std::size_t end = dot == std::string_view::npos ? name.size() : dot;
                std::string_view part = name.substr(start, end - start);

                if(!firstPart) {
                    quoted.push_back('.');
                }

                quoted += quoteSegment(part, dialect);
                firstPart = false;

                if(dot == std::string_view::npos) {
                    break;
                }

                start = dot + 1;
            }

            return quoted;
        }

    } // namespace detail

    Identifier::Identifier(std::string plain, std::string quoted, Dialect dialect)
        : plain_(std::move(plain)), quoted_(std::move(quoted)), dialect_(dialect)
    {
    }

    Result<Identifier> Identifier::parse(std::string_view name, Dialect dialect)
    {
        if(name.empty()) {
            std::string message = "identifier must not be empty";
            return Result<Identifier>::failure(Error(ErrorCode::InvalidIdentifier, 0, std::move(message)));
        }

        if(name.size() > maxIdentifierBytes) {
            std::string message = "identifier exceeds the configured length limit";
            return Result<Identifier>::failure(Error(ErrorCode::IdentifierTooLong, 0, std::move(message)));
        }

        std::size_t partCount = 0;
        std::size_t start = 0;

        while(start <= name.size()) {
            std::size_t dot = name.find('.', start);
            std::size_t end = dot == std::string_view::npos ? name.size() : dot;
            std::string_view part = name.substr(start, end - start);
            Result<void> validation = detail::validateSegment(part, start);

            if(!validation) {
                return Result<Identifier>::failure(std::move(validation.error()));
            }

            ++partCount;

            if(partCount > maxIdentifierParts) {
                std::string message = "identifier has too many dot-separated parts";
                return Result<Identifier>::failure(Error(ErrorCode::InvalidIdentifier, start, std::move(message)));
            }

            if(dot == std::string_view::npos) {
                break;
            }

            start = dot + 1;
        }

        std::string plain(name);
        std::string quoted = detail::quoteIdentifier(name, dialect);
        return Result<Identifier>::success(Identifier(std::move(plain), std::move(quoted), dialect));
    }

    Result<Identifier> Identifier::fromAllowlist(std::string_view name, const std::vector<std::string>& allowlist, Dialect dialect)
    {
        Result<Identifier> parsed = Identifier::parse(name, dialect);

        if(!parsed) {
            return parsed;
        }

        const bool allowed = std::any_of(allowlist.begin(), allowlist.end(), [name](const std::string& candidate) {
            return std::string_view(candidate) == name;
        });

        if(!allowed) {
            std::string message = "identifier is not present in the application allowlist";
            return Result<Identifier>::failure(Error(ErrorCode::IdentifierNotAllowed, 0, std::move(message)));
        }

        return parsed;
    }

    const std::string& Identifier::plain() const noexcept
    {
        return plain_;
    }

    const std::string& Identifier::quoted() const noexcept
    {
        return quoted_;
    }

    Dialect Identifier::dialect() const noexcept
    {
        return dialect_;
    }

} // namespace sqlguard
