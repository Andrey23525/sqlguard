# Driver Integration

SQLGuard produces two things:

- `RenderedQuery::sql()`: SQL with dialect-native placeholders;
- `RenderedQuery::parameters()`: ordered `Value` objects.

The adapter must pass both to the driver through its native bind API. No formatting of values into SQL is allowed.

## Mapping `Value`

```cpp
#include <sqlguard/sqlguard.h>

#include <string>
#include <type_traits>
#include <variant>

struct DriverParam {
    bool isNull = false;
    bool isText = false;
    std::string text;
    std::int64_t integer = 0;
    long double real = 0.0L;
    std::vector<std::byte> blob;
};

DriverParam toDriver(const sqlguard::Value& value)
{
    DriverParam result;
    if (value.isNull()) {
        result.isNull = true;
        return result;
    }

    std::visit([&result](const auto& stored) {
        using T = std::decay_t<decltype(stored)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            result.isNull = true;
        } else if constexpr (std::is_same_v<T, bool>) {
            result.integer = stored ? 1 : 0;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            result.integer = stored;
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
            result.text = std::to_string(stored);
            result.isText = true;
        } else if constexpr (std::is_same_v<T, long double>) {
            result.real = stored;
        } else if constexpr (std::is_same_v<T, std::string>) {
            result.text = stored;
            result.isText = true;
        } else {
            result.blob = stored;
        }
    }, value.storage());

    return result;
}
```

The exact representation depends on the driver. Some drivers distinguish signed integers, unsigned integers, decimal strings, and floating point. Map each `ValueType` to the driver's most precise matching type. Do not convert everything to a string unless the driver requires it; type coercion can change comparison and index behavior.

`Value::real` uses `long double`. If the driver only accepts `double`, convert at the adapter boundary and document precision loss. Do not use `qreal`.

## sqlite3

```cpp
sqlite3_stmt* statement = nullptr;
sqlite3_prepare_v2(db, rendered.sql().c_str(), -1, &statement, nullptr);

for (std::size_t i = 0; i < rendered.parameters().size(); ++i) {
    const sqlguard::Value& value = rendered.parameters()[i];
    const int index = static_cast<int>(i + 1);
    switch (value.type()) {
    case sqlguard::ValueType::Null:
        sqlite3_bind_null(statement, index);
        break;
    case sqlguard::ValueType::Integer:
        sqlite3_bind_int64(statement, index,
                           std::get<std::int64_t>(value.storage()));
        break;
    case sqlguard::ValueType::Real:
        sqlite3_bind_double(statement, index,
                            static_cast<double>(std::get<long double>(value.storage())));
        break;
    case sqlguard::ValueType::Text:
        sqlite3_bind_text(statement, index,
                          std::get<std::string>(value.storage()).c_str(),
                          -1, SQLITE_TRANSIENT);
        break;
    case sqlguard::ValueType::Blob: {
        const auto& blob = std::get<sqlguard::Value::Blob>(value.storage());
        sqlite3_bind_blob(statement, index, blob.data(),
                          static_cast<int>(blob.size()), SQLITE_TRANSIENT);
        break;
    }
    default:
        // Map Boolean and UnsignedInteger according to application policy.
        break;
    }
}
```

The sketch omits error checking for brevity. Production code must check every `sqlite3_*` return code, handle `SQLITE_BUSY`, and finalize the statement on every path.

## PostgreSQL via libpq

`RenderedQuery::sql()` already uses `$1`, `$2`, so it can be passed to `PQexecParams` directly:

```cpp
std::vector<const char*> values;
std::vector<int> lengths;
std::vector<int> formats;

for (const auto& parameter : rendered.parameters()) {
    // Convert each Value to a textual or binary representation owned by this scope.
    values.push_back(/* pointer to stable storage */);
    lengths.push_back(/* byte length or 0 for text */);
    formats.push_back(/* 0 = text, 1 = binary */);
}

PGresult* result = PQexecParams(
    connection,
    rendered.sql().c_str(),
    static_cast<int>(rendered.parameters().size()),
    /* paramTypes */ nullptr,
    values.data(),
    lengths.data(),
    formats.data(),
    /* resultFormat */ 0);
```

Keep every pointer alive until `PQexecParams` returns. For text format, `nullptr` in `values` means SQL NULL. `PQexecParams` supports only one statement per call; SQLGuard's default single-statement rule aligns with that.

## MySQL prepared statements

Use `mysql_stmt_prepare` with `?` placeholders, then bind `MYSQL_BIND` entries by slot order. Map:

- integer to `MYSQL_TYPE_LONGLONG`;
- unsigned integer to `MYSQL_TYPE_LONGLONG` with `is_unsigned = true`;
- real to `MYSQL_TYPE_DOUBLE` or `MYSQL_TYPE_NEWDECIMAL` depending on precision needs;
- text to `MYSQL_TYPE_STRING` with an explicit length;
- blob to `MYSQL_TYPE_BLOB`;
- NULL to `is_null = true`.

Use `mysql_stmt_execute`, not string interpolation.

## ODBC

Use `SQLBindParameter` with the correct C type and SQL type:

- `SQL_C_SBIGINT` / `SQL_BIGINT`;
- `SQL_C_UBIGINT` / `SQL_BIGINT` with an application-level range check;
- `SQL_C_DOUBLE` / `SQL_DOUBLE`;
- `SQL_C_CHAR` / `SQL_VARCHAR` with length;
- `SQL_C_BINARY` / `SQL_VARBINARY`;
- NULL with `SQL_NULL_DATA`.

ODBC uses `?` positional parameters. `RenderedQuery::sql()` for `Dialect::Standard` and `Dialect::SqlServer` keeps `?`.

## Transactions and retries

Parameter binding does not change transaction semantics:

- prepare and bind inside the same connection scope;
- do not share a `Statement`'s mutable binding state across threads;
- if a transaction retries, re-render or reuse an immutable `RenderedQuery`;
- do not log raw parameter values by default; they can contain credentials or personal data;
- log `Statement::sql()` (template only) and parameter count for diagnostics.

## Error handling

```cpp
auto parsed = sqlguard::Statement::parse(sql);
if (!parsed) {
    logger.error("SQL parse failed: code={} position={} message={}",
                 sqlguard::toString(parsed.error().code),
                 parsed.error().position,
                 parsed.error().message);
    return;
}

auto bound = parsed.value().bindAll(std::move(values));
if (!bound) {
    logger.error("SQL bind failed: code={} message={}",
                 sqlguard::toString(bound.error().code),
                 bound.error().message);
    return;
}

auto rendered = parsed.value().render();
if (!rendered) {
    logger.error("SQL render failed: code={} message={}",
                 sqlguard::toString(rendered.error().code),
                 rendered.error().message);
    return;
}
```

Do not return `parsed.error().message` to an untrusted client. It can disclose schema or query structure. Map errors to a generic external error and keep details server-side.

## Connection pool boundaries

- Parse the template once per query shape and reuse the immutable template; store bindings per request.
- If a `Statement` is cached, do not share its mutable binding state. Cache the SQL template or a factory instead.
- For PostgreSQL, prepared statement caching by name is driver-specific. SQLGuard does not manage server-side prepares.
- Ensure the connection uses the same SQL mode and dialect assumptions as `StatementOptions`.

## Testing an adapter

Test the adapter, not only SQLGuard:

- verify each `ValueType` maps to the expected driver type;
- verify NULL survives as NULL, not as empty string;
- verify text with quotes, NUL bytes, and Unicode is passed unchanged;
- verify a rendered SQL string never contains the test payload;
- verify error paths finalize statements and release handles;
- run against a real database in integration tests when possible.
