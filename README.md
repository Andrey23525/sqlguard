# SQLGuard

A C++17 library for SQL injection defense. The primary mechanism is strict separation of SQL structure and data: values travel to the driver separately and are never concatenated into SQL text.

```cpp
#include <sqlguard/sqlguard.h>

auto statement = sqlguard::Statement::parse(
    "SELECT id, email FROM users WHERE email = ? AND active = ?");
if (!statement) {
    return handle(statement.error());
}

statement.value().bind(1, sqlguard::Value::text(userInput));
statement.value().bind(2, sqlguard::Value::boolean(true));

auto rendered = statement.value().render();
if (!rendered) {
    return handle(rendered.error());
}
db.execute(rendered.value().sql(), rendered.value().parameters());
```

`userInput` may contain `' OR '1'='1`; it remains a bind value and never becomes SQL syntax.

## Components

| Component | Purpose |
|---|---|
| `Statement` / `RenderedQuery` | SQL template with `?`, typed bind parameters, multi-statement blocked by default |
| `Identifier` | strict validation and dialect quoting for identifiers; allowlist support |
| `Value` | NULL, bool, int64, uint64, long double, text, blob |
| `Detector` | heuristic scanning of suspicious constructs; defense-in-depth, not a security boundary |
| `Result<T>` / `Error` | errors without exceptions in the public API |

## Build

Requires CMake 3.20+ and a C++17 compiler. By default, both libraries are built: `sqlguard` (shared) and `sqlguard_static` (static).

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows with MSVC:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-and-test.ps1
```

Install and verify the package:

```powershell
cmake --install build --prefix build/install
powershell -ExecutionPolicy Bypass -File scripts/verify-install.ps1
```

Consume the installed package:

```cmake
find_package(sqlguard 1.0 CONFIG REQUIRED)
target_link_libraries(app PRIVATE sqlguard::shared)  # or sqlguard::static
```

## Documentation

- [Documentation index](docs/README.md)
- [Getting started and build](docs/getting-started.md)
- [Architecture](docs/architecture.md)
- [Full API](docs/api.md)
- [Security model and guarantees](docs/security-model.md)
- [SQL template syntax](docs/sql-templates.md)
- [Detector and its limits](docs/detector.md)
- [Driver integration](docs/integration.md)
- [Testing](docs/testing.md)
- [Migration from concatenation](docs/migration.md)
- [FAQ](docs/faq.md)
- [Class diagram](docs/class_diagram.md)

## Security in one paragraph

SQLGuard helps only when SQL structure is fixed in code and untrusted values are passed through `Value`. Table names, column names, and sort direction cannot be bound; use `Identifier::fromAllowlist` for them. `Detector` can help with logs and input boundaries, but it does not replace parameterization. See [docs/security-model.md](docs/security-model.md).

## License

MIT, see [LICENSE](LICENSE).
