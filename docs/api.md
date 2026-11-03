# API Reference

Header: `#include <sqlguard/sqlguard.h>` for everything, or include individual headers:

- `sqlguard/result.h`
- `sqlguard/error.h`
- `sqlguard/value.h`
- `sqlguard/dialect.h`
- `sqlguard/identifier.h`
- `sqlguard/statement.h`
- `sqlguard/detector.h`

All public names are in `namespace sqlguard`.

## Error handling: `Result<T>` and `Error`

### `enum class ErrorCode`

| Value | Meaning |
|---|---|
| `None` | no error |
| `EmptySql` | template has no SQL content |
| `EmbeddedNull` | template contains a NUL byte |
| `UnterminatedLiteral` | string, quoted identifier, or dollar quote is not closed |
| `UnterminatedComment` | block comment is not closed |
| `TooManyParameters` | more than `maxParameters` placeholders |
| `StatementNotAllowed` | multiple statements with `allowMultipleStatements == false` |
| `PlaceholderOutOfRange` | bind index is 0 or above the slot count |
| `DuplicateBinding` | slot already bound |
| `BindingCountMismatch` | `bindAll` received the wrong number of values |
| `UnboundParameter` | `render` called with at least one empty slot |
| `InvalidIdentifier` | identifier grammar violation |
| `IdentifierTooLong` | identifier exceeds the length limit |
| `IdentifierNotAllowed` | name absent from the allowlist |

### `const char* toString(ErrorCode) noexcept`

Returns a stable lowercase identifier such as `"unbound_parameter"`.

### `struct Error`

| Field | Type | Meaning |
|---|---|---|
| `code` | `ErrorCode` | error category; `None` for success |
| `position` | `std::size_t` | byte offset in the input, or `noPosition` |
| `message` | `std::string` | diagnostic text; not localized |

`inline constexpr std::size_t noPosition` marks an unavailable offset.

### `template <typename T> class Result`

Value-or-error container.

| Method | Behavior |
|---|---|
| `static Result success(T value)` | creates a successful result |
| `static Result failure(Error error)` | creates a failed result |
| `bool hasValue() const noexcept` | true when a value exists |
| `explicit operator bool() const noexcept` | same as `hasValue` |
| `T& value() &` / `const T& value() const&` / `T&& value() &&` | value access; precondition: `hasValue() == true` |
| `Error& error() &` / `const Error& error() const&` | error access; meaningful when failed |

`value()` on a failed result has undefined behavior. This is deliberate: the API avoids exceptions and expects callers to check.

### `Result<void>`

Same interface with `static Result success() noexcept` and `void value() const noexcept`. `error()` is meaningful only when failed.

## `Value`

Header: `sqlguard/value.h`.

```cpp
enum class ValueType { Null, Boolean, Integer, UnsignedInteger, Real, Text, Blob };
```

| Factory | Stored type |
|---|---|
| `Value::null()` / default constructor | SQL NULL |
| `Value::boolean(bool)` | `bool` |
| `Value::integer(std::int64_t)` | signed 64-bit |
| `Value::unsignedInteger(std::uint64_t)` | unsigned 64-bit |
| `Value::real(long double)` | `long double` |
| `Value::text(std::string_view)` | `std::string` |
| `Value::blob(Blob)` | `std::vector<std::byte>` |

`Value::Storage` is:

```cpp
std::variant<std::monostate, bool, std::int64_t, std::uint64_t, long double,
             std::string, Blob>
```

| Method | Behavior |
|---|---|
| `ValueType type() const noexcept` | logical type |
| `bool isNull() const noexcept` | true for NULL |
| `bool isNumeric() const noexcept` | true for integer, unsigned integer, real |
| `const Storage& storage() const noexcept` | adapter-facing payload |
| `bool operator==(const Value&) const` | type and payload equality |
| `bool operator!=(const Value&) const` | negation |

`Value::real` stores the value as provided. The adapter is responsible for rejecting NaN or infinity if its driver cannot represent them. `Value::text` stores bytes; SQLGuard does not validate UTF-8 or normalize Unicode.

## `Dialect`

```cpp
enum class Dialect { Standard, PostgreSql, MySql, SqlServer, Sqlite };
const char* toString(Dialect) noexcept;
```

Controls identifier quoting and lexer behavior. See [sql-templates.md](sql-templates.md).

## `Identifier`

Header: `sqlguard/identifier.h`.

```cpp
inline constexpr std::size_t maxIdentifierBytes = 128;
inline constexpr std::size_t maxIdentifierParts = 4;

class Identifier {
public:
    static Result<Identifier> parse(std::string_view name,
                                    Dialect dialect = Dialect::Standard);
    static Result<Identifier> fromAllowlist(
        std::string_view name,
        const std::vector<std::string>& allowlist,
        Dialect dialect = Dialect::Standard);

    const std::string& plain() const noexcept;
    const std::string& quoted() const noexcept;
    Dialect dialect() const noexcept;
};
```

### Grammar

- one to four dot-separated parts;
- each part starts with an ASCII letter or underscore;
- remaining characters: ASCII letters, digits, underscores;
- total length at most 128 bytes;
- no quote characters, backticks, brackets, wildcards, whitespace, semicolons, or NUL.

Examples:

| Input | Standard | MySQL | SQL Server |
|---|---|---|---|
| `users` | `"users"` | `` `users` `` | `[users]` |
| `public.users` | `"public"."users"` | `` `public`.`users` `` | `[public].[users]` |
| `dbo.users.id` | `"dbo"."users"."id"` | `` `dbo`.`users`.`id` `` | `[dbo].[users].[id]` |

### Allowlist

`fromAllowlist` first validates the identifier, then requires an exact, case-sensitive match with an element of `allowlist`. It does not normalize case or Unicode. Store the same form that the query will use.

Errors: `InvalidIdentifier`, `IdentifierTooLong`, `IdentifierNotAllowed`.

`plain()` returns the unquoted dot-separated input; `quoted()` returns the dialect-quoted form; `dialect()` returns the quoting dialect.

## `Statement`

Header: `sqlguard/statement.h`.

```cpp
inline constexpr std::size_t maxParameters = 65535;

struct StatementOptions {
    Dialect dialect = Dialect::Standard;
    bool allowMultipleStatements = false;
};

class Statement {
public:
    static Result<Statement> parse(
        std::string sql,
        const StatementOptions& options = StatementOptions {});

    const std::string& sql() const noexcept;
    std::size_t parameterCount() const noexcept;
    std::size_t boundCount() const noexcept;
    Dialect dialect() const noexcept;

    Result<void> bind(std::size_t parameterNumber, Value value);
    Result<void> bindAll(std::vector<Value> values);
    void clearBindings() noexcept;
    Result<RenderedQuery> render() const;
};
```

### `parse`

Parses the template, counts placeholders, and validates single-statement policy. It does not contact a database and does not validate SQL grammar.

Errors: `EmptySql`, `EmbeddedNull`, `UnterminatedLiteral`, `UnterminatedComment`, `TooManyParameters`, `StatementNotAllowed`.

### `sql`

Returns the original template with `?` placeholders. Values are never present.

### `parameterCount`

Number of placeholder slots. The count is fixed at parse time.

### `boundCount`

Number of slots currently containing a value. It does not validate that all slots are bound.

### `dialect`

The dialect passed in `StatementOptions`.

### `bind`

Binds a 1-based slot. Each slot can be bound once.

Errors: `PlaceholderOutOfRange`, `DuplicateBinding`.

### `bindAll`

Replaces all bindings with `values` in slot order. Requires exactly `parameterCount()` values. On count mismatch, existing bindings are unchanged.

Errors: `BindingCountMismatch`.

### `clearBindings`

Resets all slots without changing the template or count.

### `render`

Requires all slots bound. Returns `RenderedQuery` with:

- SQL for the configured dialect; for PostgreSQL, placeholders become `$1`, `$2`, ...;
- parameters in slot order.

Errors: `UnboundParameter`.

## `RenderedQuery`

Header: `sqlguard/statement.h`.

| Method | Behavior |
|---|---|
| `const std::string& sql() const noexcept` | dialect-native SQL text |
| `const std::vector<Value>& parameters() const noexcept` | bind values in slot order |

Pass `sql()` and `parameters()` to the driver separately. Never format values into `sql()`.

## `Detector`

Header: `sqlguard/detector.h`. See [detector.md](detector.md) for behavior, scoring, and limits.

```cpp
enum class Severity { None, Low, Medium, High, Critical };
enum class Signal {
    SqlComment, StackedStatement, Tautology, UnionSelect,
    TimeBasedFunction, FileAccess, CommandExecution, SystemCatalog,
    EncodedPayload, DestructiveStatement, TruncatedInput
};

struct DetectorOptions {
    bool detectComments = true;
    bool detectStackedStatements = true;
    bool detectTautologies = true;
    bool detectUnionSelect = true;
    bool detectTimeBasedFunctions = true;
    bool detectFileAndCommandAccess = true;
    bool detectSystemCatalogs = true;
    bool detectEncodedPayloads = true;
    bool detectDestructiveStatements = true;
    std::size_t maxInputBytes = 65536;
    Severity alertThreshold = Severity::Medium;
};

class Detector {
public:
    explicit Detector(DetectorOptions options = DetectorOptions {});
    DetectionResult analyze(std::string_view untrustedInput) const;
    const DetectorOptions& options() const noexcept;
};
```

### `Detector::Detector`

Copies the options. No I/O, no allocation beyond the option storage.

### `Detector::analyze`

Analyzes at most `maxInputBytes` bytes. It normalizes ASCII case, collapses whitespace, strips comments for matching, and runs the enabled detectors. It does not mutate input.

### `Detector::options`

Returns the immutable configuration of this detector.

### `Finding`

| Field | Meaning |
|---|---|
| `Signal signal` | matched category |
| `Severity severity` | static category severity |
| `std::size_t position` | byte offset in the analyzed input |
| `std::size_t length` | matched length |
| `std::string evidence` | short normalized excerpt |
| `const char* explanation` | static description |
| `const char* remediation` | static recommendation |

### `DetectionResult`

| Method | Behavior |
|---|---|
| `bool suspicious() const noexcept` | severity at or above `alertThreshold` |
| `Severity severity() const noexcept` | aggregate severity |
| `long double score() const noexcept` | additive score in `[0, 1]` |
| `bool truncated() const noexcept` | input exceeded `maxInputBytes` |
| `const std::vector<Finding>& findings() const noexcept` | findings sorted by position |

### `toString(Severity)`, `toString(Signal)`

Return stable lowercase names such as `"high"` and `"union_select"`.

## Limits

| Limit | Value | Enforcement |
|---|---|---|
| `maxParameters` | 65,535 | `Statement::parse` |
| `maxIdentifierBytes` | 128 | `Identifier::parse` |
| `maxIdentifierParts` | 4 | `Identifier::parse` |
| `DetectorOptions::maxInputBytes` | 65,536 default | `Detector::analyze` |

## Thread safety

- `Detector::analyze` is `const` and does not mutate the detector.
- `Statement` is not synchronized. Do not bind or render one statement from multiple threads concurrently.
- `Value`, `Identifier`, `RenderedQuery`, `Error`, and `DetectionResult` are value objects; a const object is safe to read concurrently.
- Returned references are valid while the owner is alive and not modified.

## ABI note

The shared library exports classes whose members include standard-library types (`std::string`, `std::vector`, `std::variant`). Use the same compiler, standard library, runtime, and build configuration for the library and the application. Do not mix Debug and Release runtime libraries or different STL implementations across the DLL boundary. If strict ABI isolation is required, wrap SQLGuard behind a C API in the application.

## Exceptions

The public API does not explicitly throw. It returns `Result` for parser and binding failures. Standard-library allocation failure can still throw `std::bad_alloc`; exception-disabled builds are supported only if the standard library is configured accordingly. Do not rely on SQLGuard to translate allocation failures into `Result`.
