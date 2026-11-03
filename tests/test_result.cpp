#include "sqlguard/result.h"

#include <string>

#include "test_framework.h"

SG_TEST(resultCarriesValue)
{
    sqlguard::Result<int> result = sqlguard::Result<int>::success(42);
    SG_REQUIRE(result.hasValue());
    SG_CHECK(static_cast<bool>(result));
    SG_CHECK_EQ(result.value(), 42);
}

SG_TEST(resultCarriesError)
{
    sqlguard::Error error(sqlguard::ErrorCode::InvalidIdentifier, 7, "bad");
    sqlguard::Result<int> result = sqlguard::Result<int>::failure(error);
    SG_CHECK(!result.hasValue());
    SG_CHECK(!static_cast<bool>(result));
    SG_CHECK(result.error().code == sqlguard::ErrorCode::InvalidIdentifier);
    SG_CHECK_EQ(result.error().position, static_cast<std::size_t>(7));
    SG_CHECK_EQ(result.error().message, std::string("bad"));
    SG_CHECK_EQ(std::string(sqlguard::toString(result.error().code)), std::string("invalid_identifier"));
}

SG_TEST(resultVoidSuccess)
{
    sqlguard::Result<void> result = sqlguard::Result<void>::success();
    SG_CHECK(result.hasValue());
    SG_CHECK(static_cast<bool>(result));
}

SG_TEST(resultVoidFailure)
{
    sqlguard::Result<void> result = sqlguard::Result<void>::failure(sqlguard::Error(sqlguard::ErrorCode::EmptySql, 0, "empty"));
    SG_CHECK(!result.hasValue());
    SG_CHECK(!static_cast<bool>(result));
    SG_CHECK_EQ(std::string(sqlguard::toString(result.error().code)), std::string("empty_sql"));
}
