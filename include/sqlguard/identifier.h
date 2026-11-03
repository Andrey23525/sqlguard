#ifndef SQLGUARD_IDENTIFIER_H
#define SQLGUARD_IDENTIFIER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "sqlguard/dialect.h"
#include "sqlguard/export.h"
#include "sqlguard/result.h"

namespace sqlguard {

    /** Maximum accepted identifier length in bytes. */
    inline constexpr std::size_t maxIdentifierBytes = 128;

    /** Maximum number of dot-separated identifier parts. */
    inline constexpr std::size_t maxIdentifierParts = 4;

    /** Validated, dialect-quoted identifier with optional allowlist check. */
    class SQLGUARD_API Identifier {
    public:
        /** Validates and quotes an identifier such as "public.users". */
        static Result<Identifier> parse(std::string_view name, Dialect dialect = Dialect::Standard);

        /** Requires an exact, case-sensitive allowlist match, then quotes it. */
        static Result<Identifier> fromAllowlist(std::string_view name, const std::vector<std::string>& allowlist,
            Dialect dialect = Dialect::Standard);

        /** Returns the unquoted, dot-separated input. */
        const std::string& plain() const noexcept;

        /** Returns the dialect-quoted representation. */
        const std::string& quoted() const noexcept;

        /** Returns the quoting dialect. */
        Dialect dialect() const noexcept;

    private:
        Identifier(std::string plain, std::string quoted, Dialect dialect);

        std::string plain_;
        std::string quoted_;
        Dialect dialect_;
    };

} // namespace sqlguard

#endif
