#ifndef SQLGUARD_SQL_SCANNER_H
#define SQLGUARD_SQL_SCANNER_H

#include <cstddef>
#include <string_view>
#include <vector>

#include "sqlguard/dialect.h"
#include "sqlguard/result.h"

namespace sqlguard::detail {

    /** Structural facts extracted from a SQL template. */
    struct SqlScan {
        /** True when the template contains something other than separators. */
        bool hasStatementContent = false;

        /** Number of '?' placeholders outside quoted regions. */
        std::size_t parameterCount = 0;

        /** Byte offsets of placeholders in the original template. */
        std::vector<std::size_t> parameterPositions;

        /** Number of top-level semicolons. */
        std::size_t semicolonCount = 0;

        /** Byte offset of the last significant token. */
        std::size_t lastSignificantPosition = 0;

        /** Byte offset of the last top-level semicolon. */
        std::size_t lastSemicolonPosition = 0;
    };

    /** Lexes a SQL template for placeholders, terminators and basic validity. */
    Result<SqlScan> scanSql(std::string_view sql, Dialect dialect);

} // namespace sqlguard::detail

#endif
