#include "sqlguard/value.h"

#include <utility>

namespace sqlguard {

    Value Value::null() noexcept
    {
        return Value();
    }

    Value Value::boolean(bool value) noexcept
    {
        return Value(Storage(value));
    }

    Value Value::integer(std::int64_t value) noexcept
    {
        return Value(Storage(value));
    }

    Value Value::unsignedInteger(std::uint64_t value) noexcept
    {
        return Value(Storage(value));
    }

    Value Value::real(long double value) noexcept
    {
        return Value(Storage(value));
    }

    Value Value::text(std::string_view value)
    {
        return Value(Storage(std::string(value)));
    }

    Value Value::blob(Blob value)
    {
        return Value(Storage(std::move(value)));
    }

    ValueType Value::type() const noexcept
    {
        switch(storage_.index()) {
            case 0:
                return ValueType::Null;
            case 1:
                return ValueType::Boolean;
            case 2:
                return ValueType::Integer;
            case 3:
                return ValueType::UnsignedInteger;
            case 4:
                return ValueType::Real;
            case 5:
                return ValueType::Text;
            case 6:
                return ValueType::Blob;
            default:
                return ValueType::Null;
        }
    }

    bool Value::isNull() const noexcept
    {
        return std::holds_alternative<std::monostate>(storage_);
    }

    bool Value::isNumeric() const noexcept
    {
        if(std::holds_alternative<std::int64_t>(storage_)) {
            return true;
        }
        if(std::holds_alternative<std::uint64_t>(storage_)) {
            return true;
        }
        return std::holds_alternative<long double>(storage_);
    }

    const Value::Storage& Value::storage() const noexcept
    {
        return storage_;
    }

    bool Value::operator==(const Value& other) const
    {
        return storage_ == other.storage_;
    }

    bool Value::operator!=(const Value& other) const
    {
        return !(*this == other);
    }

    Value::Value(Storage storage) noexcept : storage_(std::move(storage))
    {
    }

} // namespace sqlguard
