# Getting Started

## Requirements

- CMake 3.20 or newer.
- A C++17 compiler: MSVC 2019+, GCC 9+, or Clang 10+.
- For the Windows scripts: PowerShell 5.1+ and Visual Studio with the C++ toolset.
- No external dependencies.

Verified in this repository: Windows, MSVC 19.51, CMake 4.4, Ninja 1.13. Other compilers were not tested in this session, but the code uses no platform extensions.

## Build both variants

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows without a preconfigured MSVC environment:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-and-test.ps1
```

The script calls `scripts/vsenv.cmd`, builds both variants, runs CTest, then launches `sqlguard_tests_shared.exe`, `sqlguard_tests_static.exe`, and `sqlguard_demo.exe` as live processes.

Artifacts:

- `build/bin/sqlguard.dll` and `build/lib/sqlguard.lib`: shared library and import library;
- `build/lib/sqlguard_static.lib`: static library;
- `build/bin/sqlguard_tests_shared.exe`, `build/bin/sqlguard_tests_static.exe`;
- `build/bin/sqlguard_demo.exe`.

On Linux/macOS the names are conventional: `libsqlguard.so`/`libsqlguard.dylib`, and `libsqlguard.a` for the static target (`OUTPUT_NAME sqlguard_static`).

## CMake options

| Option | Default | Meaning |
|---|---|---|
| `SQLGUARD_BUILD_SHARED` | `ON` | build the shared library |
| `SQLGUARD_BUILD_STATIC` | `ON` | build the static library |
| `SQLGUARD_BUILD_TESTS` | `ON` at project root | build the CTest suite |
| `SQLGUARD_BUILD_EXAMPLES` | `ON` at project root | build `sqlguard_demo` |

At least one of `SQLGUARD_BUILD_SHARED` and `SQLGUARD_BUILD_STATIC` must be enabled. For one variant only:

```powershell
cmake -S . -B build-static -G Ninja `
  -DSQLGUARD_BUILD_SHARED=OFF `
  -DSQLGUARD_BUILD_STATIC=ON
```

## CMake targets

| Variant | Build target | Installed target | Artifact |
|---|---|---|---|
| shared | `sqlguard` | `sqlguard::shared` | DLL/so plus import library |
| static | `sqlguard_static` | `sqlguard::static` | `.lib`/`.a` |

The static target publishes `SQLGUARD_STATIC`; on Windows this removes `dllimport` from the public headers.

## Use with `add_subdirectory`

```cmake
add_subdirectory(third_party/sqlguard)
target_link_libraries(my_app PRIVATE sqlguard::static)
```

For the shared variant:

```cmake
target_link_libraries(my_app PRIVATE sqlguard::shared)
```

## Use an installed package

```powershell
cmake --install build --prefix C:/libs/sqlguard
```

```cmake
find_package(sqlguard 1.0 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE sqlguard::shared)
```

Verify both exported targets:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify-install.ps1
```

The script installs into `build/install`, builds `tests/package_consumer` through `find_package`, runs both executables, and resolves the DLL through `PATH`.

## First query

```cpp
#include <sqlguard/sqlguard.h>

#include <string>

int run(const std::string& email)
{
    auto parsed = sqlguard::Statement::parse(
        "SELECT id, email FROM users WHERE email = ? AND active = ?");
    if (!parsed) {
        return 1;
    }

    auto& statement = parsed.value();

    auto emailBinding = statement.bind(1, sqlguard::Value::text(email));
    if (!emailBinding) {
        return 2;
    }

    auto activeBinding = statement.bind(2, sqlguard::Value::boolean(true));
    if (!activeBinding) {
        return 3;
    }

    auto rendered = statement.render();
    if (!rendered) {
        return 4;
    }

    return db.execute(rendered.value().sql(), rendered.value().parameters());
}
```

The full compilable example is [examples/demo.cpp](../examples/demo.cpp).

## Execution

```cpp
auto rendered = statement.render();
if (!rendered) {
    return handle(rendered.error());
}
connection.execute(rendered.value().sql(), rendered.value().parameters());
```

For PostgreSQL, `RenderedQuery::sql()` contains `$1`, `$2`, and so on. For other dialects it contains `?`. Values stay separate in `parameters()` and are passed to the driver as bind parameters.

## Read next

- [Security model](security-model.md): required before adoption.
- [API](api.md): every method and error code.
- [SQL template syntax](sql-templates.md): lexer limits.
- [Integration](integration.md): mapping `Value` to driver parameters.
