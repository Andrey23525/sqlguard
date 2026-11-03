#include "sqlguard/detector.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sqlguard {

    namespace detector_detail {

        constexpr long double lowWeight = 0.10L;
        constexpr long double mediumWeight = 0.35L;
        constexpr long double highWeight = 0.60L;
        constexpr long double criticalWeight = 0.90L;
        constexpr std::size_t evidenceLimit = 96;
        constexpr std::size_t noMatch = static_cast<std::size_t>(-1);

        struct NormalizedText {
            std::string text;
            std::vector<std::size_t> originalOffsets;
        };

        bool isAsciiSpace(char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
        }

        bool isAsciiDigit(char value) noexcept
        {
            return value >= '0' && value <= '9';
        }

        bool isWordChar(char value) noexcept
        {
            const bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
            const bool digit = isAsciiDigit(value);
            return letter || digit || value == '_' || value == '$';
        }

        char asciiLower(char value) noexcept
        {
            if(value >= 'A' && value <= 'Z') {
                return static_cast<char>(value - 'A' + 'a');
            }

            return value;
        }

        std::size_t skipSpaces(const std::string& text, std::size_t position)
        {
            while(position < text.size() && text[position] == ' ') {
                ++position;
            }

            return position;
        }

        bool wordAt(const std::string& text, std::size_t position, std::string_view word)
        {
            if(position + word.size() > text.size()) {
                return false;
            }

            if(text.compare(position, word.size(), word) != 0) {
                return false;
            }

            if(position > 0 && isWordChar(text[position - 1])) {
                return false;
            }

            const std::size_t end = position + word.size();
            return end >= text.size() || !isWordChar(text[end]);
        }

        std::size_t findWord(const std::string& text, std::string_view word, std::size_t start = 0)
        {
            std::size_t position = start;

            while(position < text.size()) {
                const std::size_t found = text.find(word, position);

                if(found == std::string::npos) {
                    return noMatch;
                }

                if(wordAt(text, found, word)) {
                    return found;
                }

                position = found + 1;
            }

            return noMatch;
        }

        std::string_view readWord(const std::string& text, std::size_t position)
        {
            const std::size_t start = position;

            while(position < text.size() && isWordChar(text[position])) {
                ++position;
            }

            return std::string_view(text).substr(start, position - start);
        }

        std::size_t findWordSequence(const std::string& text, std::initializer_list<std::string_view> words)
        {
            const std::string_view first = *words.begin();
            std::size_t position = findWord(text, first);

            while(position != noMatch) {
                std::size_t cursor = position;
                bool matched = true;

                for(const std::string_view word : words) {
                    cursor = skipSpaces(text, cursor);

                    if(!wordAt(text, cursor, word)) {
                        matched = false;
                        break;
                    }

                    cursor += word.size();
                }

                if(matched) {
                    return position;
                }

                position = findWord(text, first, position + 1);
            }

            return noMatch;
        }

        std::size_t phraseEnd(const std::string& text, std::size_t start, std::initializer_list<std::string_view> words)
        {
            std::size_t cursor = start;

            for(const std::string_view word : words) {
                cursor = skipSpaces(text, cursor);
                cursor += word.size();
            }

            return cursor;
        }

        std::size_t findFunctionCall(const std::string& text, std::string_view name, std::size_t start = 0)
        {
            std::size_t position = start;

            while(position < text.size()) {
                const std::size_t found = text.find(name, position);

                if(found == std::string::npos) {
                    return noMatch;
                }

                const bool beforeOk = found == 0 || !isWordChar(text[found - 1]);
                const std::size_t end = found + name.size();
                const bool afterWord = end < text.size() && isWordChar(text[end]);
                const bool afterOk = !afterWord || text[end] == '.';

                if(beforeOk && afterOk) {
                    const std::size_t call = skipSpaces(text, end);

                    if(call < text.size() && text[call] == '(') {
                        return found;
                    }
                }

                position = found + 1;
            }

            return noMatch;
        }

        std::string makeEvidence(std::string_view text)
        {
            const std::size_t length = std::min(text.size(), evidenceLimit);
            std::string evidence(text.substr(0, length));

            if(length < text.size()) {
                evidence += "...";
            }

            return evidence;
        }

        std::size_t originalPosition(const NormalizedText& normalized, std::size_t position)
        {
            if(normalized.originalOffsets.empty()) {
                return 0;
            }

            if(position >= normalized.originalOffsets.size()) {
                return normalized.originalOffsets.back();
            }

            return normalized.originalOffsets[position];
        }

        bool hasSignal(const std::vector<Finding>& findings, Signal signal)
        {
            return std::any_of(findings.begin(), findings.end(), [signal](const Finding& finding) {
                return finding.signal == signal;
            });
        }

        Severity signalSeverity(Signal signal) noexcept
        {
            switch(signal) {
                case Signal::SqlComment:
                case Signal::EncodedPayload:
                case Signal::TruncatedInput:
                    return Severity::Low;
                case Signal::Tautology:
                case Signal::SystemCatalog:
                case Signal::DestructiveStatement:
                    return Severity::Medium;
                case Signal::StackedStatement:
                case Signal::UnionSelect:
                case Signal::TimeBasedFunction:
                case Signal::FileAccess:
                    return Severity::High;
                case Signal::CommandExecution:
                    return Severity::Critical;
            }
            return Severity::Low;
        }

        const char* signalExplanation(Signal signal) noexcept
        {
            switch(signal) {
                case Signal::SqlComment:
                    return "SQL comment introducer can be used to truncate a query";
                case Signal::StackedStatement:
                    return "a second SQL statement follows a terminator";
                case Signal::Tautology:
                    return "boolean expression is always true";
                case Signal::UnionSelect:
                    return "UNION SELECT can append attacker-controlled rows";
                case Signal::TimeBasedFunction:
                    return "database delay primitive is present";
                case Signal::FileAccess:
                    return "database file access primitive is present";
                case Signal::CommandExecution:
                    return "operating-system or dynamic command primitive is present";
                case Signal::SystemCatalog:
                    return "system catalog reference can expose schema metadata";
                case Signal::EncodedPayload:
                    return "percent-encoded SQL metacharacters are present";
                case Signal::DestructiveStatement:
                    return "destructive SQL statement keyword sequence is present";
                case Signal::TruncatedInput:
                    return "only the configured input prefix was analysed";
            }
            return "unknown signal";
        }

        const char* signalRemediation(Signal signal) noexcept
        {
            switch(signal) {
                case Signal::SqlComment:
                case Signal::EncodedPayload:
                    return "treat as untrusted text and keep it out of SQL syntax";
                case Signal::StackedStatement:
                case Signal::UnionSelect:
                case Signal::Tautology:
                    return "bind the value as a parameter; do not concatenate it";
                case Signal::TimeBasedFunction:
                case Signal::FileAccess:
                case Signal::CommandExecution:
                    return "block the request, bind parameters and review privileges";
                case Signal::SystemCatalog:
                    return "use a least-privilege database account and bind parameters";
                case Signal::DestructiveStatement:
                    return "bind the value and reject unexpected SQL keywords";
                case Signal::TruncatedInput:
                    return "increase maxInputBytes or analyse the full input upstream";
            }
            return "treat the input as untrusted";
        }

        void addRawFinding(std::vector<Finding>& findings, Signal signal, std::size_t position, std::size_t length, std::string evidence)
        {
            if(hasSignal(findings, signal)) {
                return;
            }

            Finding finding;
            finding.signal = signal;
            finding.severity = signalSeverity(signal);
            finding.position = position;
            finding.length = length;
            finding.evidence = std::move(evidence);
            finding.explanation = signalExplanation(signal);
            finding.remediation = signalRemediation(signal);
            findings.push_back(std::move(finding));
        }

        void addNormalizedFinding(std::vector<Finding>& findings, const NormalizedText& normalized, Signal signal, std::size_t start,
            std::size_t length)
        {
            if(length == 0 || hasSignal(findings, signal)) {
                return;
            }

            if(start >= normalized.text.size()) {
                return;
            }

            const std::size_t boundedLength = std::min(length, normalized.text.size() - start);
            const std::size_t position = originalPosition(normalized, start);
            const std::size_t lastIndex = start + boundedLength - 1;
            const std::size_t lastPosition = originalPosition(normalized, lastIndex);
            const std::size_t originalLength = lastPosition - position + 1;
            std::string evidence = makeEvidence(std::string_view(normalized.text).substr(start, boundedLength));

            addRawFinding(findings, signal, position, originalLength, std::move(evidence));
        }

        void addPhrase(std::vector<Finding>& findings, const NormalizedText& normalized, Signal signal,
            std::initializer_list<std::string_view> words)
        {
            const std::size_t start = findWordSequence(normalized.text, words);

            if(start == noMatch) {
                return;
            }

            const std::size_t end = phraseEnd(normalized.text, start, words);
            addNormalizedFinding(findings, normalized, signal, start, end - start);
        }

        NormalizedText normalize(std::string_view input, const DetectorOptions& options, std::vector<Finding>& findings)
        {
            NormalizedText normalized;
            normalized.text.reserve(input.size());
            normalized.originalOffsets.reserve(input.size());

            auto emit = [&normalized](char value, std::size_t position) {
                const char lower = asciiLower(value);

                if(lower == ' ') {
                    if(normalized.text.empty() || normalized.text.back() == ' ') {
                        return;
                    }
                }

                normalized.text.push_back(lower);
                normalized.originalOffsets.push_back(position);
            };

            std::size_t position = 0;

            while(position < input.size()) {
                const bool lineComment = input[position] == '-' && position + 1 < input.size() && input[position + 1] == '-';

                if(lineComment) {
                    std::size_t end = input.find('\n', position + 2);

                    if(end == std::string_view::npos) {
                        end = input.size();
                    }

                    if(options.detectComments) {
                        const std::size_t length = end - position;
                        addRawFinding(findings, Signal::SqlComment, position, length, makeEvidence(input.substr(position, length)));
                    }

                    emit(' ', position);
                    position = end;
                    continue;
                }

                const bool blockComment = input[position] == '/' && position + 1 < input.size() && input[position + 1] == '*';

                if(blockComment) {
                    std::size_t end = input.find("*/", position + 2);
                    std::size_t after = input.size();

                    if(end != std::string_view::npos) {
                        after = end + 2;
                    }

                    if(options.detectComments) {
                        const std::size_t length = after - position;
                        addRawFinding(findings, Signal::SqlComment, position, length, makeEvidence(input.substr(position, length)));
                    }

                    emit(' ', position);
                    position = after;
                    continue;
                }

                emit(input[position], position);
                ++position;
            }

            return normalized;
        }

        void detectStacked(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;

            for(std::size_t position = 0; position < text.size(); ++position) {
                if(text[position] != ';') {
                    continue;
                }

                const std::size_t start = skipSpaces(text, position + 1);

                if(start >= text.size()) {
                    continue;
                }

                const std::string_view word = readWord(text, start);

                if(word.empty()) {
                    continue;
                }

                const bool dangerous = word == "select" || word == "insert" || word == "update" || word == "delete" || word == "drop"
                    || word == "alter" || word == "create" || word == "truncate" || word == "grant" || word == "revoke" || word == "exec"
                    || word == "execute" || word == "call" || word == "merge" || word == "replace" || word == "attach" || word == "detach"
                    || word == "pragma" || word == "vacuum";

                if(!dangerous) {
                    continue;
                }

                addNormalizedFinding(findings, normalized, Signal::StackedStatement, position, start + word.size() - position);
            }
        }

        bool readOperand(const std::string& text, std::size_t start, std::size_t& end, std::string& value);

        void detectUnion(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            std::size_t position = findWord(text, "union");

            while(position != noMatch) {
                std::size_t cursor = skipSpaces(text, position + 5);

                if(wordAt(text, cursor, "all")) {
                    cursor = skipSpaces(text, cursor + 3);
                }
                else if(wordAt(text, cursor, "distinct")) {
                    cursor = skipSpaces(text, cursor + 8);
                }

                if(wordAt(text, cursor, "select")) {
                    const std::size_t end = cursor + 6;
                    addNormalizedFinding(findings, normalized, Signal::UnionSelect, position, end - position);
                    return;
                }

                position = findWord(text, "union", position + 1);
            }
        }

        void detectTautology(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            std::size_t position = 0;

            while(position < text.size()) {
                std::size_t found = findWord(text, "or", position);
                const std::size_t andFound = findWord(text, "and", position);

                if(found == noMatch || (andFound != noMatch && andFound < found)) {
                    found = andFound;
                }

                if(found == noMatch) {
                    return;
                }

                const bool isOr = text.compare(found, 2, "or") == 0;
                const std::size_t afterKeyword = found + (isOr ? 2 : 3);
                std::size_t leftStart = skipSpaces(text, afterKeyword);
                std::size_t leftEnd = leftStart;
                std::string leftValue;
                const bool hasLeft = readOperand(text, leftStart, leftEnd, leftValue);

                if(!hasLeft) {
                    position = found + 1;
                    continue;
                }

                std::size_t comparatorPosition = skipSpaces(text, leftEnd);
                bool comparator = false;

                if(comparatorPosition < text.size()
                    && (text[comparatorPosition] == '=' || text[comparatorPosition] == '<' || text[comparatorPosition] == '>')) {
                    comparator = true;
                    ++comparatorPosition;

                    if(comparatorPosition < text.size() && text[comparatorPosition] == '=') {
                        ++comparatorPosition;
                    }
                }
                else if(wordAt(text, comparatorPosition, "like")) {
                    comparator = true;
                    comparatorPosition += 4;
                }

                if(comparator) {
                    std::size_t rightStart = skipSpaces(text, comparatorPosition);
                    std::size_t rightEnd = rightStart;
                    std::string rightValue;
                    const bool hasRight = readOperand(text, rightStart, rightEnd, rightValue);

                    if(hasRight && leftValue == rightValue) {
                        addNormalizedFinding(findings, normalized, Signal::Tautology, found, rightEnd - found);
                        return;
                    }
                }

                position = found + 1;
            }
        }

        bool readOperand(const std::string& text, std::size_t start, std::size_t& end, std::string& value)
        {
            if(start >= text.size()) {
                return false;
            }

            const char first = text[start];

            if(first == '\'' || first == '"') {
                const char quote = first;
                std::size_t cursor = start + 1;
                value.clear();

                while(cursor < text.size()) {
                    if(text[cursor] != quote) {
                        value.push_back(text[cursor]);
                        ++cursor;
                        continue;
                    }

                    if(cursor + 1 < text.size() && text[cursor + 1] == quote) {
                        value.push_back(quote);
                        cursor += 2;
                        continue;
                    }

                    end = cursor + 1;
                    return true;
                }

                end = cursor;
                return true;
            }

            std::size_t cursor = start;

            if((first == '+' || first == '-') && start + 1 < text.size() && isAsciiDigit(text[start + 1])) {
                ++cursor;
            }

            if(cursor >= text.size() || !isAsciiDigit(text[cursor])) {
                if(wordAt(text, start, "true") || wordAt(text, start, "false")) {
                    const std::string_view word = readWord(text, start);
                    value.assign(word.begin(), word.end());
                    end = start + word.size();
                    return true;
                }

                return false;
            }

            while(cursor < text.size() && (isAsciiDigit(text[cursor]) || text[cursor] == '.')) {
                ++cursor;
            }

            value = text.substr(start, cursor - start);
            end = cursor;
            return true;
        }

        void detectTimeBased(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            const std::string_view calls[] = { "sleep", "benchmark", "pg_sleep", "pg_sleep_for", "pg_sleep_until",
                "dbms_pipe.receive_message", "dbms_lock.sleep" };

            for(const std::string_view call : calls) {
                const std::size_t found = findFunctionCall(text, call);

                if(found == noMatch) {
                    continue;
                }

                const std::size_t callOpen = skipSpaces(text, found + call.size());
                addNormalizedFinding(findings, normalized, Signal::TimeBasedFunction, found, callOpen + 1 - found);
                return;
            }

            addPhrase(findings, normalized, Signal::TimeBasedFunction, { "waitfor", "delay" });
        }

        void detectFileAccess(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            const std::string_view calls[] = { "load_file", "pg_read_file", "pg_read_binary_file", "lo_import", "openrowset",
                "opendatasource" };

            for(const std::string_view call : calls) {
                const std::size_t found = findFunctionCall(text, call);

                if(found == noMatch) {
                    continue;
                }

                const std::size_t callOpen = skipSpaces(text, found + call.size());
                addNormalizedFinding(findings, normalized, Signal::FileAccess, found, callOpen + 1 - found);
                return;
            }

            addPhrase(findings, normalized, Signal::FileAccess, { "into", "outfile" });
            addPhrase(findings, normalized, Signal::FileAccess, { "into", "dumpfile" });
            addPhrase(findings, normalized, Signal::FileAccess, { "load", "data", "infile" });
            addPhrase(findings, normalized, Signal::FileAccess, { "bulk", "insert" });
        }

        void detectCommandExecution(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            const std::string_view calls[] = { "xp_cmdshell", "sp_executesql", "exec", "execute" };

            for(const std::string_view call : calls) {
                const std::size_t found = findFunctionCall(text, call);

                if(found == noMatch) {
                    continue;
                }

                const std::size_t callOpen = skipSpaces(text, found + call.size());
                addNormalizedFinding(findings, normalized, Signal::CommandExecution, found, callOpen + 1 - found);
                return;
            }

            const std::string_view named[] = { "xp_cmdshell", "sp_executesql" };

            for(const std::string_view name : named) {
                const std::size_t found = findWord(text, name);

                if(found == noMatch) {
                    continue;
                }

                addNormalizedFinding(findings, normalized, Signal::CommandExecution, found, name.size());
                return;
            }

            addPhrase(findings, normalized, Signal::CommandExecution, { "execute", "immediate" });
        }

        void detectSystemCatalog(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            const std::string_view names[] = { "information_schema", "pg_catalog", "sqlite_master", "sqlite_schema", "sysobjects",
                "syscolumns" };
            for(const std::string_view name : names) {
                const std::size_t found = findWord(text, name);
                if(found == noMatch) {
                    continue;
                }
                addNormalizedFinding(findings, normalized, Signal::SystemCatalog, found, name.size());
                return;
            }
            const std::string_view dotted[] = { "mysql.user", "sys.objects" };
            for(const std::string_view name : dotted) {
                const std::size_t found = text.find(name);
                if(found == std::string::npos) {
                    continue;
                }
                addNormalizedFinding(findings, normalized, Signal::SystemCatalog, found, name.size());
                return;
            }
        }

        void detectDestructive(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "drop", "table" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "drop", "database" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "truncate", "table" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "alter", "table" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "delete", "from" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "insert", "into" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "create", "table" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "grant" });
            addPhrase(findings, normalized, Signal::DestructiveStatement, { "revoke" });
        }

        void detectEncoded(const NormalizedText& normalized, std::vector<Finding>& findings)
        {
            const std::string& text = normalized.text;
            const std::string_view tokens[] = { "%27", "%22", "%3d", "%3b", "%2d%2d", "%23", "%2527", "%00" };
            for(const std::string_view token : tokens) {
                const std::size_t found = text.find(token);
                if(found == std::string::npos) {
                    continue;
                }
                addNormalizedFinding(findings, normalized, Signal::EncodedPayload, found, token.size());
                return;
            }
        }

        long double findingWeight(Severity severity) noexcept
        {
            switch(severity) {
                case Severity::Low:
                    return lowWeight;
                case Severity::Medium:
                    return mediumWeight;
                case Severity::High:
                    return highWeight;
                case Severity::Critical:
                    return criticalWeight;
                case Severity::None:
                    return 0.0L;
            }
            return 0.0L;
        }

        Severity severityForScore(long double score) noexcept
        {
            if(score <= 0.0L) {
                return Severity::None;
            }

            if(score < 0.30L) {
                return Severity::Low;
            }

            if(score < 0.60L) {
                return Severity::Medium;
            }

            if(score < 0.85L) {
                return Severity::High;
            }

            return Severity::Critical;
        }

        bool severityAtLeast(Severity value, Severity threshold) noexcept
        {
            return static_cast<int>(value) >= static_cast<int>(threshold);
        }

        void collectFindings(const DetectorOptions& options, const NormalizedText& normalized, bool truncated,
            std::vector<Finding>& findings)
        {
            if(options.detectStackedStatements) {
                detectStacked(normalized, findings);
            }

            if(options.detectTautologies) {
                detectTautology(normalized, findings);
            }

            if(options.detectUnionSelect) {
                detectUnion(normalized, findings);
            }

            if(options.detectTimeBasedFunctions) {
                detectTimeBased(normalized, findings);
            }

            if(options.detectFileAndCommandAccess) {
                detectFileAccess(normalized, findings);
                detectCommandExecution(normalized, findings);
            }

            if(options.detectSystemCatalogs) {
                detectSystemCatalog(normalized, findings);
            }

            if(options.detectDestructiveStatements) {
                detectDestructive(normalized, findings);
            }

            if(options.detectEncodedPayloads) {
                detectEncoded(normalized, findings);
            }

            if(truncated) {
                addRawFinding(findings, Signal::TruncatedInput, options.maxInputBytes, 0, std::string());
            }
        }

    } // namespace detector_detail

    DetectionResult::DetectionResult(bool suspicious, Severity severity, long double score, bool truncated, std::vector<Finding> findings)
        : suspicious_(suspicious), severity_(severity), score_(score), truncated_(truncated), findings_(std::move(findings))
    {
    }

    bool DetectionResult::suspicious() const noexcept
    {
        return suspicious_;
    }

    Severity DetectionResult::severity() const noexcept
    {
        return severity_;
    }

    long double DetectionResult::score() const noexcept
    {
        return score_;
    }

    bool DetectionResult::truncated() const noexcept
    {
        return truncated_;
    }

    const std::vector<Finding>& DetectionResult::findings() const noexcept
    {
        return findings_;
    }

    Detector::Detector(DetectorOptions options) : options_(std::move(options))
    {
    }

    DetectionResult Detector::analyze(std::string_view untrustedInput) const
    {
        const std::size_t limit = options_.maxInputBytes;
        const bool truncated = untrustedInput.size() > limit;
        const std::string_view sample = untrustedInput.substr(0, limit);
        std::vector<Finding> findings;
        detector_detail::NormalizedText normalized = detector_detail::normalize(sample, options_, findings);

        detector_detail::collectFindings(options_, normalized, truncated, findings);

        std::sort(findings.begin(), findings.end(), [](const Finding& left, const Finding& right) {
            if(left.position != right.position) {
                return left.position < right.position;
            }

            return static_cast<int>(left.severity) > static_cast<int>(right.severity);
        });

        long double score = 0.0L;

        for(const Finding& finding : findings) {
            if(finding.signal == Signal::TruncatedInput) {
                continue;
            }

            score += detector_detail::findingWeight(finding.severity);
        }

        if(score > 1.0L) {
            score = 1.0L;
        }

        const Severity severity = detector_detail::severityForScore(score);
        const bool suspicious = severity != Severity::None && detector_detail::severityAtLeast(severity, options_.alertThreshold);

        return DetectionResult(suspicious, severity, score, truncated, std::move(findings));
    }

    const DetectorOptions& Detector::options() const noexcept
    {
        return options_;
    }

    const char* toString(Severity severity) noexcept
    {
        switch(severity) {
            case Severity::None:
                return "none";
            case Severity::Low:
                return "low";
            case Severity::Medium:
                return "medium";
            case Severity::High:
                return "high";
            case Severity::Critical:
                return "critical";
        }
        return "unknown";
    }

    const char* toString(Signal signal) noexcept
    {
        switch(signal) {
            case Signal::SqlComment:
                return "sql_comment";
            case Signal::StackedStatement:
                return "stacked_statement";
            case Signal::Tautology:
                return "tautology";
            case Signal::UnionSelect:
                return "union_select";
            case Signal::TimeBasedFunction:
                return "time_based_function";
            case Signal::FileAccess:
                return "file_access";
            case Signal::CommandExecution:
                return "command_execution";
            case Signal::SystemCatalog:
                return "system_catalog";
            case Signal::EncodedPayload:
                return "encoded_payload";
            case Signal::DestructiveStatement:
                return "destructive_statement";
            case Signal::TruncatedInput:
                return "truncated_input";
        }
        return "unknown";
    }

} // namespace sqlguard
