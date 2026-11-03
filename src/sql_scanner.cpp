#include "sql_scanner.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "sqlguard/error.h"

namespace sqlguard::detail {

    namespace scanner_detail {

        enum class ScanState {
            Normal,
            SingleQuoted,
            DoubleQuoted,
            BacktickQuoted,
            BracketQuoted,
            LineComment,
            BlockComment,
            DollarQuoted
        };

        bool isSqlSpace(char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
        }

        bool isIdentifierStart(char value) noexcept
        {
            const bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
            return letter || value == '_';
        }

        bool isIdentifierPart(char value) noexcept
        {
            const bool digit = value >= '0' && value <= '9';
            return isIdentifierStart(value) || digit;
        }

        class SqlScanner {
        public:
            SqlScanner(std::string_view sql, Dialect dialect) : sql_(sql), dialect_(dialect)
            {
            }

            Result<SqlScan> run()
            {
                const std::size_t nullPosition = sql_.find('\0');

                if(nullPosition != std::string_view::npos) {
                    std::string message = "SQL text contains a NUL byte";
                    return failure(ErrorCode::EmbeddedNull, nullPosition, std::move(message));
                }

                while(position_ < sql_.size()) {
                    if(state_ == ScanState::Normal) {
                        scanNormal();
                        continue;
                    }

                    scanQuotedOrComment();
                }

                if(state_ == ScanState::BlockComment) {
                    std::string message = "block comment is not terminated";
                    return failure(ErrorCode::UnterminatedComment, constructStart_, std::move(message));
                }

                if(state_ != ScanState::Normal && state_ != ScanState::LineComment) {
                    std::string message = "quoted region is not terminated";
                    return failure(ErrorCode::UnterminatedLiteral, constructStart_, std::move(message));
                }

                if(!scan_.hasStatementContent) {
                    std::string message = "SQL text is empty";
                    return failure(ErrorCode::EmptySql, 0, std::move(message));
                }

                return Result<SqlScan>::success(scan_);
            }

        private:
            Result<SqlScan> failure(ErrorCode code, std::size_t position, std::string message)
            {
                return Result<SqlScan>::failure(Error(code, position, std::move(message)));
            }

            bool hasNext(std::size_t offset = 1) const noexcept
            {
                return position_ + offset < sql_.size();
            }

            char peek(std::size_t offset = 1) const noexcept
            {
                return sql_[position_ + offset];
            }

            void mark(std::size_t position, bool content)
            {
                scan_.lastSignificantPosition = position;

                if(content) {
                    scan_.hasStatementContent = true;
                }
            }

            void beginQuote(ScanState state)
            {
                mark(position_, true);
                state_ = state;
                constructStart_ = position_;
                ++position_;
            }

            void scanNormal()
            {
                const char value = sql_[position_];

                if(isSqlSpace(value)) {
                    ++position_;
                    return;
                }

                const bool dashComment = value == '-' && hasNext() && peek() == '-'
                    && (dialect_ != Dialect::MySql || !hasNext(2) || isSqlSpace(peek(2)));

                if(dashComment) {
                    state_ = ScanState::LineComment;
                    position_ += 2;
                    return;
                }

                if(dialect_ == Dialect::MySql && value == '#') {
                    state_ = ScanState::LineComment;
                    ++position_;
                    return;
                }

                if(value == '/' && hasNext() && peek() == '*') {
                    state_ = ScanState::BlockComment;
                    constructStart_ = position_;
                    position_ += 2;
                    return;
                }

                if(value == '\'') {
                    beginQuote(ScanState::SingleQuoted);
                    return;
                }

                if(value == '"') {
                    beginQuote(ScanState::DoubleQuoted);
                    return;
                }

                if(value == '`' && (dialect_ == Dialect::MySql || dialect_ == Dialect::Sqlite)) {
                    beginQuote(ScanState::BacktickQuoted);
                    return;
                }

                if(value == '[' && dialect_ == Dialect::SqlServer) {
                    beginQuote(ScanState::BracketQuoted);
                    return;
                }

                if(value == '$' && dialect_ == Dialect::PostgreSql && beginDollarQuote()) {
                    return;
                }

                if(value == '?') {
                    mark(position_, true);
                    ++scan_.parameterCount;
                    scan_.parameterPositions.push_back(position_);
                    ++position_;
                    return;
                }

                if(value == ';') {
                    mark(position_, false);
                    ++scan_.semicolonCount;
                    scan_.lastSemicolonPosition = position_;
                    ++position_;
                    return;
                }

                mark(position_, true);
                ++position_;
            }

            bool beginDollarQuote()
            {
                std::size_t cursor = position_ + 1;

                if(cursor < sql_.size() && sql_[cursor] == '$') {
                    dollarTag_ = "$$";
                    mark(position_, true);
                    constructStart_ = position_;
                    state_ = ScanState::DollarQuoted;
                    position_ = cursor + 1;
                    return true;
                }

                if(cursor >= sql_.size() || !isIdentifierStart(sql_[cursor])) {
                    return false;
                }

                ++cursor;

                while(cursor < sql_.size() && isIdentifierPart(sql_[cursor])) {
                    ++cursor;
                }

                if(cursor >= sql_.size() || sql_[cursor] != '$') {
                    return false;
                }

                dollarTag_ = std::string(sql_.substr(position_, cursor - position_ + 1));
                mark(position_, true);
                constructStart_ = position_;
                state_ = ScanState::DollarQuoted;
                position_ = cursor + 1;
                return true;
            }

            void scanQuotedOrComment()
            {
                if(state_ == ScanState::LineComment) {
                    if(sql_[position_] == '\n') {
                        state_ = ScanState::Normal;
                    }

                    ++position_;
                    return;
                }

                if(state_ == ScanState::BlockComment) {
                    if(sql_[position_] == '*' && hasNext() && peek() == '/') {
                        state_ = ScanState::Normal;
                        position_ += 2;
                        return;
                    }

                    ++position_;
                    return;
                }

                if(state_ == ScanState::DollarQuoted) {
                    const std::size_t close = sql_.find(dollarTag_, position_);

                    if(close == std::string_view::npos) {
                        position_ = sql_.size();
                        return;
                    }

                    position_ = close + dollarTag_.size();
                    state_ = ScanState::Normal;
                    return;
                }

                scanQuoted();
            }

            void scanQuoted()
            {
                const char value = sql_[position_];

                if(dialect_ == Dialect::MySql && value == '\\' && hasNext()) {
                    position_ += 2;
                    return;
                }

                if(state_ == ScanState::BracketQuoted) {
                    if(value != ']') {
                        ++position_;
                        return;
                    }

                    if(hasNext() && peek() == ']') {
                        position_ += 2;
                        return;
                    }

                    state_ = ScanState::Normal;
                    ++position_;
                    return;
                }

                char closingQuote = '\'';

                if(state_ == ScanState::DoubleQuoted) {
                    closingQuote = '"';
                }

                if(state_ == ScanState::BacktickQuoted) {
                    closingQuote = '`';
                }

                if(value != closingQuote) {
                    ++position_;
                    return;
                }

                if(hasNext() && peek() == closingQuote) {
                    position_ += 2;
                    return;
                }

                state_ = ScanState::Normal;
                ++position_;
            }

            std::string_view sql_;
            Dialect dialect_;
            ScanState state_ = ScanState::Normal;
            std::size_t position_ = 0;
            std::size_t constructStart_ = 0;
            std::string dollarTag_;
            SqlScan scan_;
        };

    } // namespace scanner_detail

    Result<SqlScan> scanSql(std::string_view sql, Dialect dialect)
    {
        scanner_detail::SqlScanner scanner(sql, dialect);
        return scanner.run();
    }

} // namespace sqlguard::detail
