# SQL Template Syntax

## Placeholder

The template placeholder is `?`. It is recognized only outside:

- single-quoted strings;
- double-quoted strings/identifiers;
- MySQL and SQLite backtick identifiers;
- SQL Server bracket identifiers;
- line comments (`--`, and `#` in MySQL);
- block comments (`/* ... */`);
- PostgreSQL dollar-quoted strings (`$$...$$`, `$tag$...$tag$`).

A `?` inside any of those regions is ordinary text and is not counted.

```cpp
auto parsed = sqlguard::Statement::parse(
    "SELECT '?' AS marker, ? AS value -- trailing ?");
// parameterCount() == 1
```

### Placeholder numbering

Slots are 1-based, matching common database APIs and making SQL position readable.

```cpp
statement.bind(1, sqlguard::Value::text("alice"));
statement.bind(2, sqlguard::Value::integer(42));
```

Each slot can be bound once. Use `bindAll` to replace all bindings at once:

```cpp
statement.bindAll({
    sqlguard::Value::text("alice"),
    sqlguard::Value::integer(42),
});
```

`bindAll` requires the exact parameter count and leaves existing bindings unchanged on count mismatch.

## Dialect behavior

| Dialect | Placeholder input | Output from `render()` | Identifier quote | Additional lexer rules |
|---|---|---|---|---|
| `Standard` | `?` | `?` | `"name"` | standard single quotes, double-quoted identifiers, `--`, `/* */` |
| `PostgreSql` | `?` | `$1`, `$2`, ... | `"name"` | dollar-quoted strings |
| `MySql` | `?` | `?` | `` `name` `` | backticks, `#` comments, backslash escapes, `--` requires whitespace after it |
| `SqlServer` | `?` | `?` | `[name]` | bracket identifiers with `]]` escape |
| `Sqlite` | `?` | `?` | `"name"` | backticks accepted as identifier quotes |

### PostgreSQL native placeholders

PostgreSQL's libpq uses `$1`, `$2`, and so on. SQLGuard accepts `?` in the template and converts it during `render()`:

```cpp
sqlguard::StatementOptions options {sqlguard::Dialect::PostgreSql, false};
auto statement = sqlguard::Statement::parse("SELECT * FROM users WHERE id = ?", options);
statement.value().bind(1, sqlguard::Value::integer(7));

auto rendered = statement.value().render();
// rendered.value().sql() == "SELECT * FROM users WHERE id = $1"
```

`Statement::sql()` still returns the original template with `?`. `RenderedQuery::sql()` returns the dialect-native form.

### PostgreSQL JSONB `?` operator

PostgreSQL uses `?` as a JSONB existence operator:

```sql
SELECT * FROM events WHERE payload ? 'user_id';
```

SQLGuard treats that `?` as a placeholder. Use the equivalent function instead:

```cpp
// payload ? 'user_id'
"SELECT * FROM events WHERE jsonb_exists(payload, ?)"
```

The `?|` and `?&` operators have function equivalents `jsonb_exists_any` and `jsonb_exists_all`. This restriction applies only to templates parsed by SQLGuard; it prevents accidental placeholder misparsing.

### MySQL `NO_BACKSLASH_ESCAPES`

By default, MySQL treats backslash as an escape character inside strings, and so does SQLGuard's `MySql` lexer. If the server runs with `NO_BACKSLASH_ESCAPES`, a template containing a backslash-escaped quote has different boundaries on the server. The parser can then miscount placeholders, and binding fails.

Set the dialect lexer to match the server mode. If the server uses `NO_BACKSLASH_ESCAPES`, avoid backslash escapes in templates; use doubled quotes instead. A mismatch fails closed with a bind error; it does not concatenate values.

### SQLite and SQL Server

Only anonymous `?` is a placeholder. Native SQLite forms (`?1`, `:name`, `@name`, `$name`) and SQL Server `@p1` are ordinary SQL text to this lexer. Use `?` and bind by position.

## Comments

```sql
SELECT ? -- line comment with ?
FROM users
/* block comment with ? */
WHERE id = ?
```

Both `?` characters inside comments are ignored. Comments inside quoted strings are ordinary text.

Block comments are not nested. A `/*` inside an existing block comment remains comment text until the first `*/`.

## Strings and quoted identifiers

- A doubled quote escapes a quote inside a string or quoted identifier: `'it''s'`, `"a""b"`.
- In MySQL, backslash escapes are recognized inside strings: `'it\'s'`.
- SQL Server bracket identifiers escape `]` as `]]`.
- PostgreSQL dollar-quoted strings are recognized with a tag: `$$...$$`, `$body$...$body$`.
- Backtick identifiers are recognized only for MySQL and SQLite.

## Statement boundaries

By default, a template may contain at most one statement. A single trailing semicolon is allowed, optionally followed by whitespace and comments.

Accepted:

```sql
SELECT 1;
SELECT 1; -- trailing comment
SELECT 1; /* trailing comment */
```

Rejected:

```sql
SELECT 1; DROP TABLE users
SELECT 1; SELECT 2
; SELECT 1
```

To allow multiple statements, opt in explicitly:

```cpp
sqlguard::StatementOptions options;
options.allowMultipleStatements = true;
auto statement = sqlguard::Statement::parse("SELECT ?; SELECT ?", options);
```

Only enable this for a reviewed migration or batch path. Multi-statement execution is a common amplifier for injection and is disabled by default.

## Lexer limits

- A template with a NUL byte is rejected with `EmbeddedNull`.
- An unterminated string, quoted identifier, dollar quote, or block comment is rejected.
- A line comment may run to end of input; that is valid.
- The maximum number of placeholders is 65,535.
- SQL grammar, function signatures, and semantic correctness are left to the database.

## Examples

### Search with a LIKE pattern

```cpp
auto statement = sqlguard::Statement::parse(
    "SELECT id FROM products WHERE name LIKE ?");
statement.value().bind(1, sqlguard::Value::text("%" + term + "%"));
```

The `%` characters are data. They affect `LIKE` semantics as intended, but cannot alter SQL syntax.

### Dynamic sort direction

```cpp
const std::string direction =
    ascending ? "ASC" : "DESC";  // application-owned choice, not user text
const std::string sql =
    "SELECT id FROM users ORDER BY created_at " + direction;
```

If the direction must come from a request, map an enum to these literals. Do not pass user text. Use `Identifier` for the column name and an allowlist for the mapping.

### `IN` list

Build a template with the exact number of placeholders, then bind values:

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

The number of placeholders is application-owned structure. The IDs are values.
