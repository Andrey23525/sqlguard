# Class Diagram

The diagram is textual so it can be reviewed in a diff. It describes the public API and the main internal dependency.

```mermaid
classDiagram
    direction LR

    class Result~T~ {
        +success(T) Result
        +failure(Error) Result
        +hasValue() bool
        +operator bool() bool
        +value() T&
        +error() Error&
    }

    class ResultVoid {
        +success() Result
        +failure(Error) Result
        +hasValue() bool
        +operator bool() bool
        +value() void
        +error() Error&
    }

    class Error {
        +ErrorCode code
        +size_t position
        +string message
    }

    class ErrorCode {
        <<enumeration>>
        None
        EmptySql
        EmbeddedNull
        UnterminatedLiteral
        UnterminatedComment
        TooManyParameters
        StatementNotAllowed
        PlaceholderOutOfRange
        DuplicateBinding
        BindingCountMismatch
        UnboundParameter
        InvalidIdentifier
        IdentifierTooLong
        IdentifierNotAllowed
    }

    class Dialect {
        <<enumeration>>
        Standard
        PostgreSql
        MySql
        SqlServer
        Sqlite
    }

    class ValueType {
        <<enumeration>>
        Null
        Boolean
        Integer
        UnsignedInteger
        Real
        Text
        Blob
    }

    class Value {
        +null() Value
        +boolean(bool) Value
        +integer(int64) Value
        +unsignedInteger(uint64) Value
        +real(long double) Value
        +text(string_view) Value
        +blob(Blob) Value
        +type() ValueType
        +isNull() bool
        +isNumeric() bool
        +storage() Storage
        +operator==(Value) bool
    }

    class Identifier {
        +parse(string_view, Dialect) Result
        +fromAllowlist(string_view, vector, Dialect) Result
        +plain() string
        +quoted() string
        +dialect() Dialect
    }

    class StatementOptions {
        +Dialect dialect
        +bool allowMultipleStatements
    }

    class Statement {
        +parse(string, StatementOptions) Result
        +sql() string
        +parameterCount() size_t
        +boundCount() size_t
        +dialect() Dialect
        +bind(size_t, Value) ResultVoid
        +bindAll(vector) ResultVoid
        +clearBindings() void
        +render() Result
    }

    class RenderedQuery {
        +sql() string
        +parameters() vector
    }

    class Severity {
        <<enumeration>>
        None
        Low
        Medium
        High
        Critical
    }

    class Signal {
        <<enumeration>>
        SqlComment
        StackedStatement
        Tautology
        UnionSelect
        TimeBasedFunction
        FileAccess
        CommandExecution
        SystemCatalog
        EncodedPayload
        DestructiveStatement
        TruncatedInput
    }

    class DetectorOptions {
        +bool detectComments
        +bool detectStackedStatements
        +bool detectTautologies
        +bool detectUnionSelect
        +bool detectTimeBasedFunctions
        +bool detectFileAndCommandAccess
        +bool detectSystemCatalogs
        +bool detectEncodedPayloads
        +bool detectDestructiveStatements
        +size_t maxInputBytes
        +Severity alertThreshold
    }

    class Finding {
        +Signal signal
        +Severity severity
        +size_t position
        +size_t length
        +string evidence
        +const char* explanation
        +const char* remediation
    }

    class DetectionResult {
        +suspicious() bool
        +severity() Severity
        +score() long double
        +truncated() bool
        +findings() vector
    }

    class Detector {
        +Detector(DetectorOptions)
        +analyze(string_view) DetectionResult
        +options() DetectorOptions
    }

    class SqlScanner {
        <<internal>>
        +scanSql(string_view, Dialect) Result
    }

    class StatementDetail {
        <<internal>>
        +parameterLabel(size_t) string
    }

    Result~T~ --> Error
    ResultVoid --> Error
    Error --> ErrorCode
    Value --> ValueType
    Identifier --> Dialect
    Statement --> StatementOptions
    Statement --> Dialect
    Statement --> Value
    Statement --> RenderedQuery
    Statement ..> SqlScanner : parse
    Statement ..> StatementDetail : errors
    Detector --> DetectorOptions
    Detector --> DetectionResult
    DetectionResult --> Finding
    Finding --> Signal
    Finding --> Severity
```

## Relationships

- `Statement::parse` returns `Result<Statement>`. `Statement::render` returns `Result<RenderedQuery>`.
- `Statement::bind` and `Statement::bindAll` return `Result<void>`.
- `Identifier::parse` and `Identifier::fromAllowlist` return `Result<Identifier>`.
- `Value` is a tagged variant. `RenderedQuery` owns a `std::vector<Value>` in slot order.
- `Detector` owns `DetectorOptions` and returns `DetectionResult` by value. `DetectionResult` owns `std::vector<Finding>`.
- `detail::scanSql` is an internal lexer not installed as a public header. It returns `Result<SqlScan>`.

## Lifetime and ownership

| Type | Ownership |
|---|---|
| `Value` | value semantics, copyable, `long double` for real values |
| `Identifier` | value semantics; owns plain and quoted strings |
| `Statement` | value semantics; owns SQL template, placeholder offsets, and bind slots |
| `RenderedQuery` | value semantics; owns native SQL and parameter values |
| `Detector` | value semantics; owns options; `analyze` is stateless |
| `DetectionResult` | value semantics; owns findings |
| `Result<T>` | value semantics; owns either a `T` or an `Error` |

## Error path

```text
parse failure ───────────────> Result<Statement>::failure(Error)
bind failure ────────────────> Result<void>::failure(Error)
render failure ──────────────> Result<RenderedQuery>::failure(Error)
identifier failure ──────────> Result<Identifier>::failure(Error)
```

No public method throws a library-specific exception. `value()` has a precondition: the result must contain a value.
