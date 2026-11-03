#ifndef SQLGUARD_VALUE_H
#define SQLGUARD_VALUE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "sqlguard/export.h"

namespace sqlguard {

    /** Logical type of a bind parameter. */
    enum class ValueType {
        Null,
        Boolean,
        Integer,
        UnsignedInteger,
        Real,
        Text,
        Blob
    };

    /** Type-safe SQL bind value; never interpolated into SQL text. */
    class SQLGUARD_API Value {
    public:
        /** Binary payload type. */
        using Blob = std::vector<std::byte>;

        /** Underlying storage exposed to database adapters. */
        using Storage = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, long double, std::string, Blob>;

        /** Creates SQL NULL. */
        Value() noexcept = default;

        /** Creates SQL NULL. */
        static Value null() noexcept;

        /** Creates a boolean value. */
        static Value boolean(bool value) noexcept;

        /** Creates a signed 64-bit integer value. */
        static Value integer(std::int64_t value) noexcept;

        /** Creates an unsigned 64-bit integer value. */
        static Value unsignedInteger(std::uint64_t value) noexcept;

        /** Creates a floating-point value. */
        static Value real(long double value) noexcept;

        /** Creates a UTF-8 text value. */
        static Value text(std::string_view value);

        /** Creates a binary value. */
        static Value blob(Blob value);

        /** Returns the logical type. */
        ValueType type() const noexcept;

        /** True for SQL NULL. */
        bool isNull() const noexcept;

        /** True for integer, unsigned integer and real values. */
        bool isNumeric() const noexcept;

        /** Returns the adapter-facing storage. */
        const Storage& storage() const noexcept;

        /** Compares type and payload. */
        bool operator==(const Value& other) const;

        /** Compares type and payload. */
        bool operator!=(const Value& other) const;

    private:
        explicit Value(Storage storage) noexcept;

        Storage storage_;
    };

} // namespace sqlguard

#endif
