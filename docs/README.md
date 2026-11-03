# SQLGuard Documentation

Library version: 1.0.0. Standard: C++17. Build system: CMake 3.20+.

## Start here

| If you... | Read |
|---|---|
| integrate the library for the first time | [Getting started](getting-started.md), then [API](api.md) |
| design queries and a security layer | [Security model](security-model.md), [SQL template syntax](sql-templates.md) |
| write a database adapter | [Integration](integration.md), [API](api.md) |
| configure the detector for logs or a WAF | [Detector](detector.md) |
| migrate legacy concatenation code | [Migration](migration.md) |
| verify the library | [Testing](testing.md), [Class diagram](class_diagram.md) |
| need a short answer | [FAQ](faq.md) |

## Document map

### [getting-started.md](getting-started.md)

Requirements, static/shared builds, installation, `find_package`, `add_subdirectory`, target selection, and a first working example.

### [architecture.md](architecture.md)

Library layers, data flow from SQL template to `RenderedQuery`, the template lexer, the error model, and key design decisions. Links to the class diagram.

### [api.md](api.md)

Reference for every public type and method: `Result`, `Error`, `Value`, `Dialect`, `Identifier`, `Statement`, `RenderedQuery`, `Detector`, `DetectionResult`, `Finding`. Includes limits, preconditions, and error codes.

### [security-model.md](security-model.md)

What the library guarantees, which attacks it addresses, what remains the application's responsibility, why there is no string-escaping API, and why the detector is not a security boundary. Includes OWASP rationale, an integration checklist, and failure-mode analysis.

### [sql-templates.md](sql-templates.md)

Rules for `?` placeholders, comments, strings, quoting, PostgreSQL dollar-quoted strings, MySQL backslash escapes, dialect limits, multi-statement protection, limits, and native placeholders (`$1` for PostgreSQL on output).

### [detector.md](detector.md)

All signals, weights, thresholds, normalization, true positives, false positives, false negatives, `DetectorOptions`, and the difference between detection and prevention.

### [integration.md](integration.md)

How to map `Value::Storage` to driver parameters, execute `RenderedQuery`, handle errors, manage transactions, log safely, and work with connection pools.

### [testing.md](testing.md)

Test coverage, CTest and live executable runs, installed-package verification, and the autotest report location.

### [migration.md](migration.md)

Step-by-step replacement of concatenation with `Statement`, dynamic column names and `ORDER BY`, and common migration mistakes.

### [faq.md](faq.md)

Short answers: can a table be a parameter, why a `?` inside a string is not counted, what to do with the PostgreSQL JSONB `?` operator, and whether the detector can replace parameterization.

### [class_diagram.md](class_diagram.md)

Text class diagram and dependency notes.

## Sources behind the design

- OWASP SQL Injection Prevention Cheat Sheet: parameterized queries as the primary defense, allowlists for query structure, escaping strongly discouraged.
- OWASP Query Parameterization Cheat Sheet: parameters do not replace table or column names.
- PostgreSQL JSON functions: the `jsonb` `?` operator conflicts with anonymous placeholders; use `jsonb_exists`.
- C/C++ driver practice: `?` for sqlite3, MySQL, and ODBC; `$n` for libpq.

Links and rationale: [security-model.md](security-model.md#sources-and-rationale).
