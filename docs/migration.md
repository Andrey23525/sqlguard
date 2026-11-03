# Migration from String Concatenation

## The unsafe pattern

```cpp
std::string sql = "SELECT id, email FROM users WHERE email = '" + email + "'";
db.query(sql);
```

An input such as `x' OR '1'='1' --` changes the query structure. Escaping the input is not a reliable fix; it depends on dialect, server mode, and driver behavior.

## Replace with a template and bind

Before:

```cpp
std::string sql = "SELECT id FROM users WHERE email = '" + email + "'";
```

After:

```cpp
auto statement = sqlguard::Statement::parse(
    "SELECT id FROM users WHERE email = ?");
if (!statement) {
    return handle(statement.error());
}

auto bound = statement.value().bind(1, sqlguard::Value::text(email));
if (!bound) {
    return handle(bound.error());
}

auto rendered = statement.value().render();
if (!rendered) {
    return handle(rendered.error());
}

return db.execute(rendered.value().sql(), rendered.value().parameters());
```

The SQL template is a compile-time constant, or at least application-owned. The email is data.

## Dynamic identifiers

Bad:

```cpp
std::string sql = "SELECT * FROM users ORDER BY " + request.column;
```

Good:

```cpp
const std::vector<std::string> sortable {"created_at", "email", "id"};
auto column = sqlguard::Identifier::fromAllowlist(
    request.column, sortable, sqlguard::Dialect::Standard);
if (!column) {
    return badRequest(column.error());
}

std::string sql =
    "SELECT * FROM users ORDER BY " + column.value().quoted();
```

The quoted identifier is now syntactically safe, and the allowlist decides which columns are permitted.

## Dynamic sort direction

Do not treat `ASC`/`DESC` as an identifier or bind parameter. Map an enum:

```cpp
enum class SortDirection { Ascending, Descending };

std::string direction(SortDirection value)
{
    return value == SortDirection::Ascending ? "ASC" : "DESC";
}
```

## `LIKE`

```cpp
auto statement = sqlguard::Statement::parse(
    "SELECT id FROM products WHERE name LIKE ?");
statement.value().bind(1, sqlguard::Value::text("%" + term + "%"));
```

The wildcard characters are data. Escape user-provided `%` and `_` only if the business rule requires literal matching. That escaping is separate from SQL syntax; it does not protect against injection.

## `IN` lists

Build one placeholder per element and bind each element:

```cpp
std::string sql = "SELECT id FROM users WHERE id IN (";
for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0) {
        sql += ", ";
    }
    sql += "?";
}
sql += ")";
```

The count is structure; the IDs are values.

## `LIMIT` and `OFFSET`

Use bind parameters where the database supports them:

```sql
SELECT id FROM users ORDER BY id LIMIT ? OFFSET ?
```

Bind integers. Do not format numeric strings into the SQL text; even numeric injection can break structure if validation is missing.

## Dynamic table names

Map the requested logical name to an application-owned table identifier:

```cpp
const std::vector<std::string> tables {"users", "orders", "audit_log"};
auto table = sqlguard::Identifier::fromAllowlist(
    requested, tables, sqlguard::Dialect::PostgreSql);
```

Do not accept arbitrary schema-qualified names. `public.users` is still a table identifier; whether the caller may read it is an authorization decision.

## String building the SQL template

Keep the template fixed. If the query shape must change, use separate constants per shape:

```cpp
const std::string byEmail =
    "SELECT id FROM users WHERE email = ?";
const std::string byEmailAndStatus =
    "SELECT id FROM users WHERE email = ? AND status = ?";
```

Do not build templates from request data. Doing so moves attacker text into the SQL grammar before SQLGuard sees it.

## Migrating a codebase

1. Find all string concatenation and `std::format`/stream SQL construction.
2. Classify each interpolated part as value or structure.
3. Move every value to `bind` / `bindAll`.
4. Move every dynamic structure to a fixed allowlist and `Identifier` or an enum.
5. Remove old escaping helpers. Do not keep a second, weaker path.
6. Add regression tests with tautologies, comments, and stacked statements.
7. Run the detector over historical logs to find attempted attacks, not to sanitize input.

## Common mistakes

- "I validated the email with a regex, so concatenation is fine." Validation does not make data into safe SQL syntax.
- "I escaped quotes." Escaping is dialect- and mode-dependent. Use binding.
- "The table name comes from a fixed dropdown." The client can send any value. Validate on the server with an allowlist.
- "The ID is an integer." Parse it as an integer and bind it; do not interpolate the raw string.
- "ORM generated the SQL, so it is safe." ORM raw-query fragments often concatenate. Check the generated SQL and bind values.
- "The detector said the input is clean." Detection is not sanitization.
