#include "sqlguard/identifier.h"

#include <string>
#include <vector>

#include "test_framework.h"

SG_TEST(identifierQuotesByDialect)
{
    const sqlguard::Result<sqlguard::Identifier> standard = sqlguard::Identifier::parse("public.users", sqlguard::Dialect::Standard);
    SG_REQUIRE(standard);
    SG_CHECK_EQ(standard.value().quoted(), std::string("\"public\".\"users\""));
    SG_CHECK_EQ(standard.value().plain(), std::string("public.users"));

    const sqlguard::Result<sqlguard::Identifier> mysql = sqlguard::Identifier::parse("app.users", sqlguard::Dialect::MySql);
    SG_REQUIRE(mysql);
    SG_CHECK_EQ(mysql.value().quoted(), std::string("`app`.`users`"));

    const sqlguard::Result<sqlguard::Identifier> sqlServer = sqlguard::Identifier::parse("dbo.users", sqlguard::Dialect::SqlServer);
    SG_REQUIRE(sqlServer);
    SG_CHECK_EQ(sqlServer.value().quoted(), std::string("[dbo].[users]"));

    const sqlguard::Result<sqlguard::Identifier> postgres = sqlguard::Identifier::parse("users", sqlguard::Dialect::PostgreSql);
    SG_REQUIRE(postgres);
    SG_CHECK_EQ(postgres.value().quoted(), std::string("\"users\""));
}

SG_TEST(identifierAllowlistAcceptsExactName)
{
    const std::vector<std::string> allowed { "email", "created_at", "id" };
    const sqlguard::Result<sqlguard::Identifier> accepted = sqlguard::Identifier::fromAllowlist("email", allowed);
    SG_REQUIRE(accepted);
    SG_CHECK_EQ(accepted.value().quoted(), std::string("\"email\""));
}

SG_TEST(identifierAllowlistRejectsUnknownName)
{
    const std::vector<std::string> allowed { "email", "created_at", "id" };
    const sqlguard::Result<sqlguard::Identifier> rejected = sqlguard::Identifier::fromAllowlist("password", allowed);
    SG_REQUIRE(!rejected);
    SG_CHECK(rejected.error().code == sqlguard::ErrorCode::IdentifierNotAllowed);
}

SG_TEST(identifierAllowlistIsCaseSensitive)
{
    const std::vector<std::string> allowed { "email" };
    const sqlguard::Result<sqlguard::Identifier> rejected = sqlguard::Identifier::fromAllowlist("Email", allowed);
    SG_REQUIRE(!rejected);
    SG_CHECK(rejected.error().code == sqlguard::ErrorCode::IdentifierNotAllowed);
}

SG_TEST(identifierRejectsInjectionSyntax)
{
    const char* invalidNames[] = { "", "users; DROP TABLE users", "\"users\"", "user-name", "1users", ".users", "users.", "users..id",
        "users id", "users/**/id", "users`; --", "users' OR '1'='1" };
    for(const char* name : invalidNames) {
        const sqlguard::Result<sqlguard::Identifier> parsed = sqlguard::Identifier::parse(name);
        SG_CHECK(!parsed);
        if(!parsed) {
            SG_CHECK(parsed.error().code == sqlguard::ErrorCode::InvalidIdentifier);
        }
    }
}

SG_TEST(identifierRejectsTooManyParts)
{
    const sqlguard::Result<sqlguard::Identifier> parsed = sqlguard::Identifier::parse("a.b.c.d.e");
    SG_REQUIRE(!parsed);
    SG_CHECK(parsed.error().code == sqlguard::ErrorCode::InvalidIdentifier);
}

SG_TEST(identifierRejectsOverlongName)
{
    const std::string name(sqlguard::maxIdentifierBytes + 1, 'a');
    const sqlguard::Result<sqlguard::Identifier> parsed = sqlguard::Identifier::parse(name);
    SG_REQUIRE(!parsed);
    SG_CHECK(parsed.error().code == sqlguard::ErrorCode::IdentifierTooLong);
}

SG_TEST(identifierSupportsMaxDepth)
{
    const sqlguard::Result<sqlguard::Identifier> parsed = sqlguard::Identifier::parse("a.b.c.d");
    SG_REQUIRE(parsed);
    SG_CHECK_EQ(parsed.value().quoted(), std::string("\"a\".\"b\".\"c\".\"d\""));
}
