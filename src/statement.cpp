#include "sqlguard/statement.h"

#include <string>
#include <utility>

#include "sql_scanner.h"

namespace sqlguard {

    namespace statement_detail {

        std::string parameterLabel(std::size_t parameterNumber)
        {
            return "parameter " + std::to_string(parameterNumber);
        }

    } // namespace statement_detail

    Statement::Statement(std::string sql, std::vector<std::size_t> parameterPositions, Dialect dialect)
        : sql_(std::move(sql)), parameterPositions_(std::move(parameterPositions)), dialect_(dialect), bindings_(parameterPositions_.size())
    {
    }

    Result<Statement> Statement::parse(std::string sql, const StatementOptions& options)
    {
        Result<detail::SqlScan> scanResult = detail::scanSql(sql, options.dialect);

        if(!scanResult) {
            return Result<Statement>::failure(std::move(scanResult.error()));
        }

        const detail::SqlScan& scan = scanResult.value();

        if(!options.allowMultipleStatements && scan.semicolonCount > 0) {
            const bool trailingTerminator = scan.semicolonCount == 1 && scan.lastSignificantPosition == scan.lastSemicolonPosition;

            if(!trailingTerminator) {
                std::string message = "multiple SQL statements are disabled by StatementOptions";
                return Result<Statement>::failure(Error(ErrorCode::StatementNotAllowed, scan.lastSemicolonPosition, std::move(message)));
            }
        }

        if(scan.parameterCount > maxParameters) {
            std::string message = "SQL template exceeds the maximum parameter count";
            return Result<Statement>::failure(Error(ErrorCode::TooManyParameters, noPosition, std::move(message)));
        }

        return Result<Statement>::success(Statement(std::move(sql), std::move(scanResult.value().parameterPositions), options.dialect));
    }

    const std::string& Statement::sql() const noexcept
    {
        return sql_;
    }

    std::size_t Statement::parameterCount() const noexcept
    {
        return parameterPositions_.size();
    }

    std::size_t Statement::boundCount() const noexcept
    {
        std::size_t count = 0;

        for(const std::optional<Value>& binding : bindings_) {
            if(binding.has_value()) {
                ++count;
            }
        }

        return count;
    }

    Dialect Statement::dialect() const noexcept
    {
        return dialect_;
    }

    Result<void> Statement::bind(std::size_t parameterNumber, Value value)
    {
        if(parameterNumber == 0 || parameterNumber > parameterCount()) {
            std::string message = statement_detail::parameterLabel(parameterNumber) + " is outside the template range";
            return Result<void>::failure(Error(ErrorCode::PlaceholderOutOfRange, noPosition, std::move(message)));
        }

        std::optional<Value>& slot = bindings_[parameterNumber - 1];

        if(slot.has_value()) {
            std::string message = statement_detail::parameterLabel(parameterNumber) + " is already bound";
            return Result<void>::failure(Error(ErrorCode::DuplicateBinding, noPosition, std::move(message)));
        }

        slot.emplace(std::move(value));
        return Result<void>::success();
    }

    Result<void> Statement::bindAll(std::vector<Value> values)
    {
        if(values.size() != parameterCount()) {
            std::string message = "expected " + std::to_string(parameterCount()) + " values, received " + std::to_string(values.size());
            return Result<void>::failure(Error(ErrorCode::BindingCountMismatch, noPosition, std::move(message)));
        }

        std::vector<std::optional<Value>> bindings;
        bindings.reserve(values.size());

        for(Value& value : values) {
            bindings.emplace_back(std::move(value));
        }

        bindings_ = std::move(bindings);
        return Result<void>::success();
    }

    void Statement::clearBindings() noexcept
    {
        for(std::optional<Value>& binding : bindings_) {
            binding.reset();
        }
    }

    Result<RenderedQuery> Statement::render() const
    {
        std::vector<Value> parameters;
        parameters.reserve(parameterCount());

        for(std::size_t index = 0; index < parameterCount(); ++index) {
            const std::optional<Value>& binding = bindings_[index];

            if(!binding.has_value()) {
                std::string message = statement_detail::parameterLabel(index + 1) + " is not bound";
                return Result<RenderedQuery>::failure(Error(ErrorCode::UnboundParameter, noPosition, std::move(message)));
            }

            parameters.push_back(*binding);
        }

        std::string nativeSql;

        if(dialect_ != Dialect::PostgreSql) {
            nativeSql = sql_;
        }
        else {
            nativeSql.reserve(sql_.size() + parameterCount() * 2);
            std::size_t copied = 0;

            for(std::size_t index = 0; index < parameterCount(); ++index) {
                const std::size_t placeholder = parameterPositions_[index];
                nativeSql.append(sql_, copied, placeholder - copied);
                nativeSql.push_back('$');
                nativeSql += std::to_string(index + 1);
                copied = placeholder + 1;
            }

            nativeSql.append(sql_, copied, std::string::npos);
        }

        return Result<RenderedQuery>::success(RenderedQuery(std::move(nativeSql), std::move(parameters)));
    }

    RenderedQuery::RenderedQuery(std::string sql, std::vector<Value> parameters) : sql_(std::move(sql)), parameters_(std::move(parameters))
    {
    }

    const std::string& RenderedQuery::sql() const noexcept
    {
        return sql_;
    }

    const std::vector<Value>& RenderedQuery::parameters() const noexcept
    {
        return parameters_;
    }

} // namespace sqlguard
