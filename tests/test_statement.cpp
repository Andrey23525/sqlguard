#include "sqlguard/statement.h"

#include <string>
#include <utility>
#include <vector>

#include "test_framework.h"

SG_TEST(statementParsesAndRendersParameters)
{
    sqlguard::Result<sqlguard::Statement> parsed = sqlguard::Statement::parse("SELECT id, email FROM users WHERE email = ? AND active = ?");
    SG_REQUIRE(parsed);
    sqlguard::Statement statement = std::move(parsed.value());
    SG_CHECK_EQ(statement.parameterCount(), static_cast<std::size_t>(2));
    SG_CHECK_EQ(statement.boundCount(), static_cast<std::size_t>(0));
    SG_CHECK_EQ(statement.sql(), std::string("SELECT id, email FROM users WHERE email = ? AND active = ?"));

    const std::string attack = "' OR '1'='1";
    SG_CHECK(statement.bind(1, sqlguard::Value::text(attack)));
    SG_CHECK(statement.bind(2, sqlguard::Value::boolean(true)));
    SG_CHECK_EQ(statement.boundCount(), static_cast<std::size_t>(2));

    sqlguard::Result<sqlguard::RenderedQuery> rendered = statement.render();
    SG_REQUIRE(rendered);
    SG_CHECK_EQ(rendered.value().sql(), statement.sql());
    SG_REQUIRE(rendered.value().parameters().size() == 2);
    SG_CHECK(rendered.value().parameters()[0] == sqlguard::Value::text(attack));
    SG_CHECK(rendered.value().parameters()[1] == sqlguard::Value::boolean(true));
    SG_CHECK(rendered.value().sql().find(attack) == std::string::npos);
}

SG_TEST(statementIgnoresPlaceholdersInQuotedTextAndComments)
{
    sqlguard::Result<sqlguard::Statement> quoted = sqlguard::Statement::parse("SELECT '?' AS marker, ? AS value -- trailing ?");
    SG_REQUIRE(quoted);
    SG_CHECK_EQ(quoted.value().parameterCount(), static_cast<std::size_t>(1));

    sqlguard::Result<sqlguard::Statement> blockComment = sqlguard::Statement::parse("SELECT ? /* ? */ , ?");
    SG_REQUIRE(blockComment);
    SG_CHECK_EQ(blockComment.value().parameterCount(), static_cast<std::size_t>(2));

    sqlguard::Result<sqlguard::Statement> lineContinuation = sqlguard::Statement::parse("SELECT ? -- ?\n, ?");
    SG_REQUIRE(lineContinuation);
    SG_CHECK_EQ(lineContinuation.value().parameterCount(), static_cast<std::size_t>(2));
}

SG_TEST(statementUsesDialectSpecificQuoting)
{
    const sqlguard::StatementOptions postgres { sqlguard::Dialect::PostgreSql, false };
    sqlguard::Result<sqlguard::Statement> dollar = sqlguard::Statement::parse("SELECT $$?$$, ?", postgres);
    SG_REQUIRE(dollar);
    SG_CHECK_EQ(dollar.value().parameterCount(), static_cast<std::size_t>(1));

    const sqlguard::StatementOptions sqlServer { sqlguard::Dialect::SqlServer, false };
    sqlguard::Result<sqlguard::Statement> bracket = sqlguard::Statement::parse("SELECT [a?b], ?", sqlServer);
    SG_REQUIRE(bracket);
    SG_CHECK_EQ(bracket.value().parameterCount(), static_cast<std::size_t>(1));

    const sqlguard::StatementOptions mysql { sqlguard::Dialect::MySql, false };
    sqlguard::Result<sqlguard::Statement> escapedQuote = sqlguard::Statement::parse("SELECT 'it\\'s ?' , ?", mysql);
    SG_REQUIRE(escapedQuote);
    SG_CHECK_EQ(escapedQuote.value().parameterCount(), static_cast<std::size_t>(1));
}

SG_TEST(statementRendersPostgreSqlNativePlaceholders)
{
    const sqlguard::StatementOptions options { sqlguard::Dialect::PostgreSql, false };
    sqlguard::Result<sqlguard::Statement> parsed = sqlguard::Statement::parse("SELECT ?, ?", options);
    SG_REQUIRE(parsed);
    sqlguard::Statement statement = std::move(parsed.value());
    SG_REQUIRE(statement.bindAll({ sqlguard::Value::integer(1), sqlguard::Value::text("two") }));

    const sqlguard::Result<sqlguard::RenderedQuery> rendered = statement.render();
    SG_REQUIRE(rendered);
    SG_CHECK_EQ(rendered.value().sql(), std::string("SELECT $1, $2"));
    SG_CHECK_EQ(statement.sql(), std::string("SELECT ?, ?"));
}

SG_TEST(statementRejectsDuplicateAndOutOfRangeBindings)
{
    sqlguard::Result<sqlguard::Statement> parsed = sqlguard::Statement::parse("SELECT ?");
    SG_REQUIRE(parsed);
    sqlguard::Statement statement = std::move(parsed.value());
    SG_CHECK(statement.bind(1, sqlguard::Value::integer(1)));

    const sqlguard::Result<void> duplicate = statement.bind(1, sqlguard::Value::integer(2));
    SG_REQUIRE(!duplicate);
    SG_CHECK(duplicate.error().code == sqlguard::ErrorCode::DuplicateBinding);

    const sqlguard::Result<void> tooLow = statement.bind(0, sqlguard::Value::integer(2));
    SG_REQUIRE(!tooLow);
    SG_CHECK(tooLow.error().code == sqlguard::ErrorCode::PlaceholderOutOfRange);

    const sqlguard::Result<void> tooHigh = statement.bind(2, sqlguard::Value::integer(2));
    SG_REQUIRE(!tooHigh);
    SG_CHECK(tooHigh.error().code == sqlguard::ErrorCode::PlaceholderOutOfRange);
}

SG_TEST(statementRejectsUnboundRender)
{
    sqlguard::Result<sqlguard::Statement> parsed = sqlguard::Statement::parse("SELECT ?");
    SG_REQUIRE(parsed);
    const sqlguard::Result<sqlguard::RenderedQuery> rendered = parsed.value().render();
    SG_REQUIRE(!rendered);
    SG_CHECK(rendered.error().code == sqlguard::ErrorCode::UnboundParameter);
}

SG_TEST(statementBindAllRequiresExactCount)
{
    sqlguard::Result<sqlguard::Statement> parsed = sqlguard::Statement::parse("SELECT ?, ?");
    SG_REQUIRE(parsed);
    sqlguard::Statement statement = std::move(parsed.value());

    const sqlguard::Result<void> tooFew = statement.bindAll({ sqlguard::Value::integer(1) });
    SG_REQUIRE(!tooFew);
    SG_CHECK(tooFew.error().code == sqlguard::ErrorCode::BindingCountMismatch);
    SG_CHECK_EQ(statement.boundCount(), static_cast<std::size_t>(0));

    const sqlguard::Result<void> exact = statement.bindAll({ sqlguard::Value::integer(1), sqlguard::Value::text("two") });
    SG_REQUIRE(exact);
    SG_CHECK_EQ(statement.boundCount(), static_cast<std::size_t>(2));

    statement.clearBindings();
    SG_CHECK_EQ(statement.boundCount(), static_cast<std::size_t>(0));
}

SG_TEST(statementRejectsMultipleStatementsByDefault)
{
    const sqlguard::Result<sqlguard::Statement> rejected = sqlguard::Statement::parse("SELECT 1; DROP TABLE users");
    SG_REQUIRE(!rejected);
    SG_CHECK(rejected.error().code == sqlguard::ErrorCode::StatementNotAllowed);

    const sqlguard::Result<sqlguard::Statement> trailing = sqlguard::Statement::parse("SELECT 1; -- trailing comment");
    SG_REQUIRE(trailing);
    SG_CHECK_EQ(trailing.value().parameterCount(), static_cast<std::size_t>(0));

    const sqlguard::StatementOptions permissive { sqlguard::Dialect::Standard, true };
    const sqlguard::Result<sqlguard::Statement> accepted = sqlguard::Statement::parse("SELECT ?; SELECT ?", permissive);
    SG_REQUIRE(accepted);
    SG_CHECK_EQ(accepted.value().parameterCount(), static_cast<std::size_t>(2));
}

SG_TEST(statementRejectsMalformedSql)
{
    const sqlguard::Result<sqlguard::Statement> literal = sqlguard::Statement::parse("SELECT 'unterminated");
    SG_REQUIRE(!literal);
    SG_CHECK(literal.error().code == sqlguard::ErrorCode::UnterminatedLiteral);

    const sqlguard::Result<sqlguard::Statement> comment = sqlguard::Statement::parse("SELECT 1 /* unterminated");
    SG_REQUIRE(!comment);
    SG_CHECK(comment.error().code == sqlguard::ErrorCode::UnterminatedComment);

    const sqlguard::Result<sqlguard::Statement> empty = sqlguard::Statement::parse("  -- nothing here");
    SG_REQUIRE(!empty);
    SG_CHECK(empty.error().code == sqlguard::ErrorCode::EmptySql);

    std::string nullSql = "SELECT 1";
    nullSql.push_back('\0');
    const sqlguard::Result<sqlguard::Statement> nul = sqlguard::Statement::parse(nullSql);
    SG_REQUIRE(!nul);
    SG_CHECK(nul.error().code == sqlguard::ErrorCode::EmbeddedNull);
}
