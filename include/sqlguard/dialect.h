#ifndef SQLGUARD_DIALECT_H
#define SQLGUARD_DIALECT_H

#include "sqlguard/export.h"

namespace sqlguard {

    /** SQL dialect used for identifier quoting and lexer rules. */
    enum class Dialect {
        Standard,
        PostgreSql,
        MySql,
        SqlServer,
        Sqlite
    };

    /** Stable name of a dialect. */
    SQLGUARD_API const char* toString(Dialect dialect) noexcept;

} // namespace sqlguard

#endif
