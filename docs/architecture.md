# Architecture

## Goal

SQLGuard makes it difficult to turn untrusted data into SQL syntax. It does not parse or rewrite arbitrary application SQL. It provides small, explicit primitives:

1. a parsed SQL template whose value slots are separate from the SQL text;
2. typed bind values;
3. validated, quoted identifiers for the few SQL structure elements that cannot be bound;
4. a bounded heuristic detector for defense-in-depth.

## Layers

```
Application
    |
    |  Statement::parse(sql template, StatementOptions)
    v
+---------------------+       +---------------------+
| Statement           |       | Identifier          |
|  immutable SQL      |       |  validate + quote   |
|  placeholder slots  |       |  optional allowlist |
|  bindings           |       +---------------------+
+----------+----------+
           |
           |  bind(1..N, Value)
           v
+---------------------+       +---------------------+
| RenderedQuery       |       | Detector            |
|  dialect-native SQL |       |  normalized input   |
|  values vector      |       |  signals + score    |
+----------+----------+       +---------------------+
           |
           v
Database driver receives SQL and values separately
```

`Detector` is independent of the query path. It must not be used as a gate that decides whether a value is safe to concatenate; no value is ever concatenated by this library.

## Component responsibilities

### `Value`

A tagged, type-safe value:

- SQL NULL;
- boolean;
- signed 64-bit integer;
- unsigned 64-bit integer;
- `long double`;
- UTF-8 text bytes;
- binary blob.

The database adapter reads `Value::storage()` and converts it to the driver representation. SQLGuard does not choose the wire format; the adapter does.

### `Statement` and `RenderedQuery`

`Statement::parse` runs a deterministic lexer over the SQL template. It counts `?` placeholders outside strings, quoted identifiers, line comments, block comments, MySQL `#` comments, and PostgreSQL dollar-quoted strings. It also detects top-level semicolons for multi-statement protection.

The parsed `Statement` stores:

- the original SQL template (`sql()`);
- byte offsets of each placeholder;
- one optional binding per slot;
- the dialect.

`render()` verifies that every slot is bound, copies values in slot order, and builds the dialect-native SQL. For PostgreSQL, each placeholder becomes `$1`, `$2`, and so on. Other dialects keep `?`.

`RenderedQuery` is a value object holding SQL and the ordered bind values. The two must be passed to the driver together. Passing values through string formatting defeats the design.

### `Identifier`

Identifiers cannot be represented by bind parameters. `Identifier::parse` accepts a strict subset of SQL identifier syntax and applies the dialect quote:

- Standard, PostgreSQL, SQLite: `"identifier"`;
- MySQL: `` `identifier` ``;
- SQL Server: `[identifier]`.

`Identifier::fromAllowlist` additionally requires an exact case-sensitive match in an application-provided list. The allowlist is the application's decision about which identifiers may appear in a query; quoting is only syntax protection.

### `Detector`

The detector lowercases a bounded prefix, removes comments for matching, and applies fixed pattern detectors:

- comments;
- stacked statements;
- tautologies;
- `UNION SELECT`;
- time-based functions;
- file access;
- command execution;
- system catalogs;
- percent-encoded SQL metacharacters;
- destructive statement sequences;
- truncation.

It produces findings, a sum of weights, a severity, and a boolean against a configurable threshold. It never changes or sanitizes the input. See [detector.md](detector.md).

## Error model

C++17 has no `std::expected`. SQLGuard uses `Result<T>`, a value-or-`Error` container:

- no explicit exceptions in the public API;
- errors have a stable `ErrorCode`, an optional byte position, and a diagnostic string;
- callers must check `Result` before accessing `value()`;
- `value()` on a failed result is undefined behavior, not an exception;
- allocation failure from the standard library is outside this contract.

This model keeps error handling visible at each call site and works with builds that disable exceptions.

## Fail-closed parsing

A SQL template is rejected when:

- it is empty;
- it contains a NUL byte;
- a quoted literal, quoted identifier, dollar-quoted string, or block comment is not terminated;
- it contains more than one statement while multi-statement is disabled;
- it exceeds 65,535 placeholders;
- a placeholder is bound twice, out of range, or left unbound at render time;
- an identifier is invalid or absent from the allowlist.

A malformed template is not "repaired". Rejection is intentional: silent repairs create a gap between what the developer wrote and what the database executes.

## Dialect lexer

The lexer is not a SQL grammar parser. It recognizes only the lexical constructs that affect placeholder counting and statement boundaries:

- single-quoted strings with doubled-quote escapes;
- double-quoted strings/identifiers with doubled-quote escapes;
- MySQL backslash escapes in quoted strings;
- MySQL/SQLite backticks;
- SQL Server brackets with `]]` escapes;
- `--` line comments; MySQL requires whitespace or end-of-input after `--`;
- MySQL `#` line comments;
- `/* ... */` block comments;
- PostgreSQL `$tag$ ... $tag$`;
- `?` placeholders;
- `;` terminators.

Anything else is treated as ordinary SQL text. This is sufficient for safe value separation, but it is not a validator for SQL syntax or semantics. The database remains the authority on SQL correctness.

## Why no escaping API

OWASP strongly discourages escaping as a substitute for parameterization. Escaping depends on dialect and server configuration. MySQL's `NO_BACKSLASH_ESCAPES` changes string semantics; manual escape libraries must replicate every server mode and every driver behavior. A single mistake creates an injection point.

SQLGuard therefore does not provide `escapeString` or `quoteLiteral`. If a legacy driver cannot bind a value, use the driver's own literal escaping API or change the interface. Do not build an escaping layer around this library.

## Build architecture

Both artifacts are compiled from the same source list:

- `sqlguard` (`SHARED`): public symbols exported with `SQLGUARD_API`; private symbols hidden; compile definition `SQLGUARD_BUILDING_LIBRARY`.
- `sqlguard_static` (`STATIC`): consumer-side compile definition `SQLGUARD_STATIC`.

CMake installs headers, both targets, and a package config. The static and shared variants have distinct output names on Windows to avoid a `.lib` collision.

## Thread safety and lifetime

- `Statement` is mutable while bindings are being set and is not internally synchronized.
- `Detector` is stateless during `analyze` and can be shared between threads if its options are not modified.
- `DetectionResult`, `RenderedQuery`, `Identifier`, `Value`, and `Error` are value objects.
- Returned `const&` references remain valid only while the owning object is alive and not modified.

See [class_diagram.md](class_diagram.md) for the type relationships.
