# Detector

`Detector` is a bounded, deterministic heuristic scanner for untrusted text. It is intended for logs, alerts, test fixtures, and an additional input boundary. It is **not** a SQL injection prevention mechanism.

## Contract

```cpp
sqlguard::Detector detector;
sqlguard::DetectionResult result = detector.analyze(untrustedInput);

if (result.suspicious()) {
    // log, rate-limit, or reject; never concatenate the value into SQL
}
```

The input is never modified. The detector does not know the SQL context, the value's intended type, or the database parser. A finding is evidence of a suspicious pattern, not proof of an attack.

## Signals and weights

| Signal | Severity | Weight | Default enabled |
|---|---|---|---|
| `SqlComment` | Low | 0.10 | yes |
| `StackedStatement` | High | 0.60 | yes |
| `Tautology` | Medium | 0.35 | yes |
| `UnionSelect` | High | 0.60 | yes |
| `TimeBasedFunction` | High | 0.60 | yes |
| `FileAccess` | High | 0.60 | yes |
| `CommandExecution` | Critical | 0.90 | yes |
| `SystemCatalog` | Medium | 0.35 | yes |
| `EncodedPayload` | Low | 0.10 | yes |
| `DestructiveStatement` | Medium | 0.35 | yes |
| `TruncatedInput` | Low | 0.00 | always when truncated |

One finding is emitted per signal; the first occurrence wins. Findings are sorted by byte position.

## Score and severity

The score is the sum of finding weights, capped at `1.0`:

| Score | Severity |
|---|---|
| `0` | `None` |
| `> 0` and `< 0.30` | `Low` |
| `>= 0.30` and `< 0.60` | `Medium` |
| `>= 0.60` and `< 0.85` | `High` |
| `>= 0.85` | `Critical` |

`DetectionResult::suspicious()` is true when the aggregate severity is at or above `DetectorOptions::alertThreshold`. The default threshold is `Medium`.

Examples:

- `plain -- text`: one Low finding, score `0.10`, severity `Low`, default `suspicious() == false`.
- `' OR '1'='1`: one Medium tautology, score `0.35`, severity `Medium`, default `suspicious() == true`.
- `1 UNION SELECT ...`: one High finding, score `0.60`, severity `High`.
- `'; DROP TABLE users; --`: stacked High + destructive Medium + comment Low, score `1.0`, severity `Critical`.
- `%27%20OR%201%3D1`: one Low encoded finding, score `0.10`, default not suspicious. The detector does not URL-decode.

## Normalization

Before pattern matching, the analyzed prefix is:

- limited to `maxInputBytes` bytes;
- converted to ASCII lowercase;
- stripped of `--` and `/* ... */` comments (they are replaced by a space for matching);
- collapsed so runs of whitespace become one space.

The original byte offsets are retained for `Finding::position`.

Consequences:

- `UNION/**/SELECT` normalizes to `union select` and is detected.
- `UN/**/ION SELECT` normalizes to `un ion select` and is **not** detected as `UNION SELECT`; only the comment signal fires (Low).
- A comment marker inside a quoted string is still stripped for matching. This can create a false positive or hide text; the detector is heuristic.
- Percent-encoded payloads are not decoded. They are reported as `EncodedPayload` (Low).

## Patterns

### `SqlComment`

Detects `--` and `/*`. A low-severity signal. Comment markers are common in prose and are not suspicious by themselves.

### `StackedStatement`

Finds a `;` followed, after optional spaces, by a statement keyword: `select`, `insert`, `update`, `delete`, `drop`, `alter`, `create`, `truncate`, `grant`, `revoke`, `exec`, `execute`, `call`, `merge`, `replace`, `attach`, `detach`, `pragma`, or `vacuum`.

### `Tautology`

Finds `OR` or `AND` followed by two operands joined by `=`, `>`, `<`, `>=`, `<=`, or `LIKE`, where the operands are equal. Operands are quoted strings, numbers, `true`, or `false`.

Detected: `' OR '1'='1`, `1 OR 1=1`, `1 AND 2=2`, `x' OR 'a' LIKE 'a`.

Not detected: `OR username = username` (identifier operands are not treated as literals), arbitrary prose without a comparator.

### `UnionSelect`

Finds `UNION`, optional `ALL` or `DISTINCT`, then `SELECT`.

### `TimeBasedFunction`

Finds calls to `sleep`, `benchmark`, `pg_sleep`, `pg_sleep_for`, `pg_sleep_until`, `dbms_pipe.receive_message`, `dbms_lock.sleep`, and the phrase `WAITFOR DELAY`.

### `FileAccess`

Finds calls to `load_file`, `pg_read_file`, `pg_read_binary_file`, `lo_import`, `openrowset`, `opendatasource`, plus phrases `INTO OUTFILE`, `INTO DUMPFILE`, `LOAD DATA INFILE`, and `BULK INSERT`.

### `CommandExecution`

Finds calls to `xp_cmdshell`, `sp_executesql`, `exec(...)`, `execute(...)`, bare `xp_cmdshell` / `sp_executesql` words, and `EXECUTE IMMEDIATE`.

### `SystemCatalog`

Finds `information_schema`, `pg_catalog`, `sqlite_master`, `sqlite_schema`, `sysobjects`, `syscolumns`, `mysql.user`, and `sys.objects`.

### `EncodedPayload`

Finds `%27`, `%22`, `%3d`, `%3b`, `%2d%2d`, `%23`, `%2527`, `%00` after lowercasing.

### `DestructiveStatement`

Finds `DROP TABLE`, `DROP DATABASE`, `TRUNCATE TABLE`, `ALTER TABLE`, `DELETE FROM`, `INSERT INTO`, `CREATE TABLE`, `GRANT`, and `REVOKE` in the normalized text.

### `TruncatedInput`

Added when `untrustedInput.size() > maxInputBytes`. It has zero weight and is reported so callers know the result covers only a prefix.

## Options

```cpp
sqlguard::DetectorOptions options;
options.detectTautologies = false;
options.alertThreshold = sqlguard::Severity::High;
options.maxInputBytes = 8192;

sqlguard::Detector detector(options);
```

Turning off a `detect*` flag removes both the signal and its weight. `maxInputBytes` bounds CPU and memory use; the detector does not allocate proportional to the full input beyond the analyzed prefix.

## Recommended use

```cpp
const std::string_view value = request.field("q");
const auto scan = detector.analyze(value);

if (scan.suspicious()) {
    logger.warn("suspicious SQL pattern: severity={} score={} signals={}",
                sqlguard::toString(scan.severity()),
                static_cast<double>(scan.score()),
                scan.findings().size());
    // optional: reject the request at the application boundary
}

// The query still binds value as data.
statement.bind(1, sqlguard::Value::text(value));
```

Do not write:

```cpp
if (!scan.suspicious()) {
    sql += value;  // forbidden: detection is not sanitization
}
```

## Known false positives

- A product description that contains `drop table` or `UNION SELECT`.
- Markdown or code samples with SQL keywords.
- Text containing `--` or `/*`.
- A legitimate `SLEEP(...)` function call in a value.
- A valid email or name that triggers a comment signal.

## Known false negatives

- Encoded payloads that are not in the fixed `%xx` list.
- Comment-obfuscated keywords such as `UN/**/ION`.
- Payloads using SQL features not in the fixed pattern set.
- Context-dependent payloads that require schema knowledge.
- Values longer than `maxInputBytes` where the payload appears after the analyzed prefix.

This is why the detector is defense-in-depth only.
