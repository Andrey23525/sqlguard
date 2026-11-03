# Testing

## Test lanes

The repository has two deterministic test lanes:

1. **Unit and contract tests**: `tests/test_*.cpp`, 36 test functions per variant, built twice, once against `sqlguard` (shared) and once against `sqlguard_static`.
2. **Installed-package consumer**: `tests/package_consumer`, built through `find_package(sqlguard 1.0 CONFIG REQUIRED)` against an installed prefix, linking both exported targets.

Both lanes are local and dependency-free. They run in seconds.

## Run with CTest

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected:

```
1/2 Test #1: sqlguard_tests_shared ... Passed
2/2 Test #2: sqlguard_tests_static ... Passed
100% tests passed, 0 tests failed out of 2
```

## Run the live executables

AGENTS.md requires a live executable comparison when autotests are requested. The build script does that:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-and-test.ps1
```

It runs:

```text
build/bin/sqlguard_tests_shared.exe
build/bin/sqlguard_tests_static.exe
build/bin/sqlguard_demo.exe
```

`sqlguard_demo.exe` demonstrates:

- a SQL template with two placeholders;
- a malicious text value bound as a parameter;
- `RenderedQuery::sql()` staying free of the value;
- PostgreSQL `$1` rendering;
- an allowlisted identifier;
- detector output.

## Verify the installed package

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify-install.ps1
```

The script:

1. configures and builds the library;
2. installs it into `build/install`;
3. configures `tests/package_consumer` with `CMAKE_PREFIX_PATH`;
4. builds `consumer_shared` and `consumer_static`;
5. runs both executables with the installed DLL on `PATH`.

The consumer calls `Statement`, `Identifier`, and `Detector`, so it validates headers, exported symbols, compile definitions, and runtime linking.

## What is covered

- `Result<T>` and `Result<void>` success/failure paths.
- Every `ValueType`, equality, storage.
- Identifier grammar, length limit, part limit, allowlists, dialect quoting.
- Statement parsing, placeholder counting in strings/comments/dollar quotes/bracket identifiers, MySQL escaped quotes.
- Bind errors: duplicate, out of range, count mismatch, unbound render.
- Multi-statement rejection, trailing semicolon, permissive mode.
- Malformed templates: unterminated literal/comment, NUL, empty.
- PostgreSQL native `$n` rendering.
- Detector signals, options, thresholds, truncation, false-positive guard for plain comments.
- Stable `toString` names.
- Package installation and both exported targets.

## What is not covered

- Live database servers. No PostgreSQL, MySQL, SQL Server, or SQLite server is started by the test suite.
- Driver-specific adapters. They are application code; test them separately against real drivers.
- SQL grammar validation. The library intentionally does not implement a full parser.
- Performance at extreme input sizes. The detector is bounded, but no benchmark budget is enforced.
- Sanitizers and fuzzing. These are good next steps for a production fork; they are not part of the current gate.

## Reports

The test report is stored under `autotests/out/`. It includes:

- environment;
- commands run;
- CTest output;
- live executable output;
- installed-package verification;
- result and known gaps.

## Determinism

The tests use no network, no database, no clock, no random values, and no concurrency. Failures reproduce from the command line.

## Adding tests

1. Add `tests/test_<area>.cpp`.
2. Register the file in `tests/CMakeLists.txt` under `sqlguardTestSources`.
3. Use the `SG_TEST`, `SG_CHECK`, `SG_REQUIRE`, and `SG_CHECK_EQ` macros from `tests/test_framework.h`.
4. Run both CTest targets; do not test only one library variant.

The test framework is intentionally tiny. Do not weaken an assertion to make a test pass; fix the implementation or the test expectation.
