# Security Model

## What SQLGuard guarantees

When used as documented, for a query built from a fixed SQL template:

1. A `Value` passed to `bind` is never inserted into the SQL string.
2. `RenderedQuery::sql()` contains only the application-owned SQL template and placeholders.
3. `RenderedQuery::parameters()` contains the values in slot order.
4. A SQL template with more than one statement is rejected unless `allowMultipleStatements` is explicitly enabled.
5. Identifiers are accepted only from a strict ASCII grammar and are quoted for the selected dialect.
6. Unknown identifiers can be rejected against an exact allowlist.
7. Malformed lexical constructs fail closed with an error.

The security boundary is the interface between SQLGuard and the database driver. The driver must receive `sql()` and `parameters()` as separate arguments, or the adapter must pass each parameter through its native bind API. If application code formats `parameters()` back into `sql()`, the guarantees are void.

## What the library does not protect

- SQL text built from untrusted input before `Statement::parse`.
- Table names, column names, and sort direction chosen directly by a user without an allowlist.
- SQL injection inside stored procedures that construct dynamic SQL from their arguments.
- Second-order injection: a value that is safe when stored and later used as part of SQL syntax.
- Authorization failures: a valid, parameterized query can still return data the caller must not see.
- Database account over-privilege, weak TLS, exposed backups, or credential leaks.
- Denial of service by expensive queries, large parameters, or connection exhaustion.
- Faulty driver behavior outside the documented bind API.

`Detector` does not extend this list. It is an advisory heuristic.

## Threats addressed

| Threat | Mechanism | Residual risk |
|---|---|---|
| `' OR '1'='1` tautology | the value is a bind parameter | none for this input path |
| `'; DROP TABLE ...` stacked statement | placeholder binding plus multi-statement rejection | driver-level multi-statement still depends on the driver |
| `UNION SELECT` data exfiltration | value stays data | if SQL is concatenated elsewhere |
| comment truncation `--` | value stays data | comments inside trusted templates are still allowed |
| time-based blind injection | value stays data | detector can flag attempts in logs |
| file read/write via functions | value stays data | database privileges determine real impact |
| dynamic `ORDER BY` | `Identifier` grammar plus allowlist | allowlist must be complete and correct |
| dynamic table/column | `Identifier` grammar plus allowlist | allowlist must be complete and correct |

## Why parameter binding

OWASP's SQL Injection Prevention Cheat Sheet identifies prepared statements with parameterized queries as the primary defense. The database parses the SQL structure first, then receives values through a separate protocol. Values are typed data, not syntax tokens. This removes the entire class of quote-balancing and keyword-smuggling failures that string escaping tries to patch.

Binding does not validate business data. A parameterized query can return every row if the application asks for every row, or update an unauthorized row if authorization is missing. Binding protects syntax, not authorization.

## Why allowlist identifiers

SQL dialects cannot bind identifiers. A table or column name is SQL structure. Quoting by itself prevents syntax breakage but does not prevent selecting `password_hash` or `information_schema.tables`. Therefore:

- validate the identifier grammar;
- quote for the dialect;
- compare against a short, application-owned set of allowed names;
- reject unknown names with `IdentifierNotAllowed`;
- keep the allowlist next to the query or in a central policy, not in user input.

The allowlist is exact and case-sensitive. If the application uses case-insensitive identifiers, normalize before calling `Identifier::fromAllowlist` and store the normalized keys. Do not rely on the database to fold case consistently across operating systems.

## Why no escaping

Escaping is dialect-specific and configuration-specific. OWASP calls escaping a strongly discouraged approach. MySQL's `NO_BACKSLASH_ESCAPES` changes which backslash sequences terminate a string. Incorrect assumptions produce either broken queries or injection.

SQLGuard deliberately offers no escaping function. Use native parameter binding where possible. For legacy code that cannot bind, use the database driver's tested literal-escaping API, and migrate that code to binding.

## Why the detector is not a boundary

The detector works on text, without knowing the SQL context, the value's type, or the database's parser. It cannot prove that input is safe:

- a benign sentence can contain `--`;
- a valid name can contain `UNION`;
- obfuscation such as `UN/**/ION SELECT` is only partially covered;
- percent-encoded input is flagged but not decoded;
- an attacker can use a payload not in the fixed pattern set.

Use it for monitoring, alerting, test fixtures, and an extra input boundary. Never use its result to decide that a value may be concatenated.

## Integration checklist

- [ ] Every query with data uses `Statement::parse` and `bind` / `bindAll`.
- [ ] No application code inserts `Value` into SQL text.
- [ ] Dynamic identifiers use `Identifier::fromAllowlist` with a short list.
- [ ] Dynamic sort direction is a separate allowlist (`ASC`/`DESC`), never an identifier.
- [ ] Multi-statement remains disabled unless a reviewed code path needs it.
- [ ] Database account follows least privilege.
- [ ] Errors do not leak SQL text, schema names, or values to untrusted callers.
- [ ] Logs record `SQL: <template>` and parameter count, not raw sensitive values.
- [ ] Tests include tautologies, comment truncation, stacked statements, and identifier attacks.
- [ ] Detector findings are monitored but never used as a "safe to concatenate" decision.

## Failure modes

1. **Template built from untrusted text.** SQLGuard sees only the final string. If the attacker controls the template, they can add syntax. Keep templates in code.
2. **Value rendered into the template before parsing.** Same failure. Do not use `std::format`, `+`, or string streams to build SQL with data.
3. **Unvalidated identifier.** Quoting prevents syntax breakage but not privilege escalation. Use the allowlist.
4. **Misconfigured dialect.** MySQL backslash parsing without `NO_BACKSLASH_ESCAPES` can miscount placeholders. The result is a parse/bind error, not silent concatenation, but the application fails. Configure the dialect to match the server.
5. **Driver misuse.** A driver that interpolates parameters client-side is outside the guarantee. Use native bind APIs.
6. **Privileged database account.** A successful injection elsewhere can still read or modify data. Least privilege limits blast radius.
7. **Untrusted templates from migrations or plugins.** Review them like code; the parser rejects structural ambiguity but does not know business intent.

## Sources and rationale

- [OWASP SQL Injection Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/SQL_Injection_Prevention_Cheat_Sheet.html): prepared statements with parameterized queries are the primary defense; allow-list input validation for structure; escaping all dynamic inputs is strongly discouraged.
- [OWASP Query Parameterization Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Query_Parameterization_Cheat_Sheet.html): parameter markers represent data, not table or column names.
- [PostgreSQL: JSON Functions and Operators](https://www.postgresql.org/docs/current/functions-json.html): `jsonb` defines `?`, `?|`, and `?&` operators. Use `jsonb_exists` / `jsonb_exists_any` / `jsonb_exists_all` in templates parsed by SQLGuard, because `?` is the library placeholder.
- [MySQL: Server SQL Modes](https://dev.mysql.com/doc/refman/8.0/en/sql-mode.html): `NO_BACKSLASH_ESCAPES` changes string escaping rules.

## Verified claims

- The core API is compiled and exercised by the CTest suites for both static and shared artifacts.
- The installed package is consumed through `find_package` by `tests/package_consumer` for both targets.
- The detector behavior in this document is covered by `tests/test_detector.cpp`.
- Claims about specific database server behavior are taken from the cited vendor documentation and are not tested against live database servers in this repository.
