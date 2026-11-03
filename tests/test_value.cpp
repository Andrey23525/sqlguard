#include "sqlguard/value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "test_framework.h"

SG_TEST(valueNullIsDefault)
{
    const sqlguard::Value value;
    SG_CHECK(value.isNull());
    SG_CHECK(value.type() == sqlguard::ValueType::Null);
    SG_CHECK(!value.isNumeric());
}

SG_TEST(valueScalarTypes)
{
    const sqlguard::Value boolean = sqlguard::Value::boolean(true);
    SG_CHECK(boolean.type() == sqlguard::ValueType::Boolean);
    SG_CHECK(!boolean.isNumeric());

    const sqlguard::Value integer = sqlguard::Value::integer(std::int64_t { -42 });
    SG_CHECK(integer.type() == sqlguard::ValueType::Integer);
    SG_CHECK(integer.isNumeric());

    const sqlguard::Value unsignedInteger = sqlguard::Value::unsignedInteger(std::uint64_t { 42 });
    SG_CHECK(unsignedInteger.type() == sqlguard::ValueType::UnsignedInteger);
    SG_CHECK(unsignedInteger.isNumeric());

    const sqlguard::Value real = sqlguard::Value::real(1.5L);
    SG_CHECK(real.type() == sqlguard::ValueType::Real);
    SG_CHECK(real.isNumeric());
}

SG_TEST(valueTextAndBlob)
{
    const sqlguard::Value text = sqlguard::Value::text("O'Reilly");
    SG_CHECK(text.type() == sqlguard::ValueType::Text);
    SG_CHECK(std::holds_alternative<std::string>(text.storage()));
    SG_CHECK_EQ(std::get<std::string>(text.storage()), std::string("O'Reilly"));

    const std::vector<std::byte> bytes { std::byte { 0x00 }, std::byte { 0xFF } };
    const sqlguard::Value blob = sqlguard::Value::blob(bytes);
    SG_CHECK(blob.type() == sqlguard::ValueType::Blob);
    SG_CHECK(std::holds_alternative<sqlguard::Value::Blob>(blob.storage()));
}

SG_TEST(valueEqualityComparesTypeAndPayload)
{
    SG_CHECK(sqlguard::Value::integer(1) == sqlguard::Value::integer(1));
    SG_CHECK(sqlguard::Value::integer(1) != sqlguard::Value::integer(2));
    SG_CHECK(sqlguard::Value::integer(1) != sqlguard::Value::unsignedInteger(1));
    SG_CHECK(sqlguard::Value::text("1") != sqlguard::Value::integer(1));
    SG_CHECK(sqlguard::Value::null() == sqlguard::Value::null());
}
