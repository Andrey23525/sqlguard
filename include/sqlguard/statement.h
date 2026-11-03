#ifndef SQLGUARD_STATEMENT_H
#define SQLGUARD_STATEMENT_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "sqlguard/dialect.h"
#include "sqlguard/export.h"
#include "sqlguard/result.h"
#include "sqlguard/value.h"

namespace sqlguard {

    class RenderedQuery;

    /** Hard limit against accidental parameter explosion. */
    inline constexpr std::size_t maxParameters = 65535;

    /** Parsing and execution policy for a Statement. */
    struct StatementOptions {
        /** Dialect used by the SQL lexer. */
        Dialect dialect = Dialect::Standard;

        /** Allow more than one statement separated by top-level semicolons. */
        bool allowMultipleStatements = false;
    };

    /** Immutable SQL template with independently bound value slots. */
    class SQLGUARD_API Statement {
    public:
        /** Parses a SQL template and counts its parameter slots. */
        static Result<Statement> parse(std::string sql, const StatementOptions& options = StatementOptions {});

        /** Returns the original SQL template with '?' placeholders. */
        const std::string& sql() const noexcept;

        /** Returns the number of parameter slots. */
        std::size_t parameterCount() const noexcept;

        /** Returns the number of currently bound slots. */
        std::size_t boundCount() const noexcept;

        /** Returns the configured dialect. */
        Dialect dialect() const noexcept;

        /** Binds a 1-based parameter slot; each slot can be bound once. */
        Result<void> bind(std::size_t parameterNumber, Value value);

        /** Binds every slot in order, replacing previous bindings. */
        Result<void> bindAll(std::vector<Value> values);

        /** Clears all bindings while keeping the parsed template. */
        void clearBindings() noexcept;

        /** Produces SQL plus the ordered bind values when every slot is bound. */
        Result<RenderedQuery> render() const;

    private:
        Statement(std::string sql, std::vector<std::size_t> parameterPositions, Dialect dialect);

        std::string sql_;
        std::vector<std::size_t> parameterPositions_;
        Dialect dialect_;
        std::vector<std::optional<Value>> bindings_;
    };

    /** SQL text and the ordered values to pass to a database driver. */
    class SQLGUARD_API RenderedQuery {
    public:
        /** Returns SQL with dialect-native placeholders ($1 for PostgreSQL). */
        const std::string& sql() const noexcept;

        /** Returns bind values in placeholder order. */
        const std::vector<Value>& parameters() const noexcept;

    private:
        friend class Statement;

        RenderedQuery(std::string sql, std::vector<Value> parameters);

        std::string sql_;
        std::vector<Value> parameters_;
    };

} // namespace sqlguard

#endif
