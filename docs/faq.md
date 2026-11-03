# FAQ

## Can a table or column name be a bind parameter?

No. Parameters represent data, not SQL structure. Use `Identifier::fromAllowlist` and quote the result for the dialect. The allowlist must be application-owned.

## Why is `?` inside `'...'` not counted as a placeholder?

Because it is string data, not a value slot. The lexer counts only placeholders outside strings, quoted identifiers, comments, and dollar-quoted strings. This prevents a literal question mark in a template from demanding an extra bind value.

## Why does PostgreSQL output `$1` instead of `?`

`libpq` and PostgreSQL prepared statements use numbered `$n` placeholders. `Statement` accepts the portable `?` template and `render()` translates it for `Dialect::PostgreSql`. Other dialects keep `?`.

## What about the PostgreSQL JSONB `?` operator?

`?` is also a JSONB operator. Use `jsonb_exists(payload, ?)`, `jsonb_exists_any`, or `jsonb_exists_all` in SQLGuard templates. A raw `payload ? 'key'` would be parsed as a placeholder, which changes the query and the parameter count.

## Can I bind a whole `IN (...)` list?

No. SQL syntax needs one placeholder per value. Build `?, ?, ?` from the application-owned count and bind each element.

## Can I use this library without exceptions?

The public API returns `Result` and does not throw explicitly. Standard-library allocation can still throw `std::bad_alloc`. If you compile with exceptions disabled, ensure your standard library and runtime support that configuration.

## Is `Detector` enough to stop SQL injection?

No. It is a heuristic. Use parameter binding for every value, identifiers only from an allowlist, and least privilege. The detector is monitoring and defense-in-depth.

## Why no escaping function?

Escaping is dialect- and configuration-dependent and is strongly discouraged as a substitute for parameterization. Use the database driver's native bind API. For legacy code, use the driver's tested literal API and migrate.

## Is a `Statement` thread-safe?

No. A `Statement` owns mutable bindings. Do not bind or render the same instance concurrently. Separate `Statement` objects and immutable `RenderedQuery` values can be used independently. `Detector::analyze` is const and does not mutate the detector.

## Can I reuse a `Statement` for several requests?

The parsed template is immutable, but bindings are part of the object. Parse once and copy the template, or create a new `Statement` per request. After `render()`, call `clearBindings()` only if the statement is not shared.

## Does SQLGuard validate SQL grammar or column names?

No. It validates lexical structure for safe placeholder counting and identifier grammar for quoting. The database validates SQL semantics. A syntactically valid but unauthorized query is an authorization problem, not an injection problem.

## Why are both static and shared libraries built by default?

The requirement is to support both. Separate targets prevent an accidental Windows `.lib` collision and allow consumers to choose. Disable one with `SQLGUARD_BUILD_SHARED=OFF` or `SQLGUARD_BUILD_STATIC=OFF`.

## Which compiler/ABI combinations are supported?

The verified build is MSVC 19.51 on Windows. The code targets C++17 and has no external dependencies. For the shared library, use the same compiler, standard library, runtime, and build configuration on both sides. See [api.md](api.md#abi-note).

## Why is there no ORM adapter?

SQLGuard is driver-agnostic. An ORM adapter would have to depend on the ORM's query model and would expand the dependency surface. The core primitives are small enough to integrate directly. If an adapter is added later, it must preserve the same rule: values never become SQL text.
