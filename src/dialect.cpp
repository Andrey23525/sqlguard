#include "sqlguard/dialect.h"

namespace sqlguard {

    const char* toString(Dialect dialect) noexcept
    {
        switch(dialect) {
            case Dialect::Standard:
                return "standard";
            case Dialect::PostgreSql:
                return "postgresql";
            case Dialect::MySql:
                return "mysql";
            case Dialect::SqlServer:
                return "sqlserver";
            case Dialect::Sqlite:
                return "sqlite";
        }
        return "unknown";
    }

} // namespace sqlguard
