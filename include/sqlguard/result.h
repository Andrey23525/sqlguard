#ifndef SQLGUARD_RESULT_H
#define SQLGUARD_RESULT_H

#include <optional>
#include <utility>

#include "sqlguard/error.h"

namespace sqlguard {

    /** Value-or-error container; check hasValue() before value(). */
    template<typename T>
    class Result {
    public:
        /** Creates a successful result. */
        static Result success(T value)
        {
            return Result(std::move(value));
        }

        /** Creates a failed result. */
        static Result failure(Error error)
        {
            return Result(std::move(error));
        }

        /** True when a value is present. */
        bool hasValue() const noexcept
        {
            return value_.has_value();
        }

        /** True when a value is present. */
        explicit operator bool() const noexcept
        {
            return hasValue();
        }

        /** Accesses the value; requires hasValue() == true. */
        T& value() & noexcept
        {
            return *value_;
        }

        /** Accesses the value; requires hasValue() == true. */
        const T& value() const& noexcept
        {
            return *value_;
        }

        /** Moves the value out; requires hasValue() == true. */
        T&& value() && noexcept
        {
            return std::move(*value_);
        }

        /** Accesses the error; meaningful when hasValue() == false. */
        Error& error() & noexcept
        {
            return error_;
        }

        /** Accesses the error; meaningful when hasValue() == false. */
        const Error& error() const& noexcept
        {
            return error_;
        }

    private:
        explicit Result(T value) : value_(std::move(value))
        {
        }

        explicit Result(Error error) : error_(std::move(error))
        {
        }

        std::optional<T> value_;
        Error error_;
    };

    /** Void specialisation for operations that cannot fail with a value. */
    template<>
    class Result<void> {
    public:
        /** Creates a successful result. */
        static Result success() noexcept
        {
            return Result(true, Error {});
        }

        /** Creates a failed result. */
        static Result failure(Error error)
        {
            return Result(false, std::move(error));
        }

        /** True when the operation succeeded. */
        bool hasValue() const noexcept
        {
            return success_;
        }

        /** True when the operation succeeded. */
        explicit operator bool() const noexcept
        {
            return success_;
        }

        /** No-op; requires hasValue() == true. */
        void value() const noexcept
        {
        }

        /** Accesses the error; meaningful when hasValue() == false. */
        Error& error() & noexcept
        {
            return error_;
        }

        /** Accesses the error; meaningful when hasValue() == false. */
        const Error& error() const& noexcept
        {
            return error_;
        }

    private:
        Result(bool success, Error error) : success_(success), error_(std::move(error))
        {
        }

        bool success_ = false;
        Error error_;
    };

} // namespace sqlguard

#endif
