#ifndef SQLGUARD_DETECTOR_H
#define SQLGUARD_DETECTOR_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "sqlguard/export.h"

namespace sqlguard {

    /** Risk level used by the heuristic detector. */
    enum class Severity {
        None,
        Low,
        Medium,
        High,
        Critical
    };

    /** Stable category of a detector finding. */
    enum class Signal {
        SqlComment,
        StackedStatement,
        Tautology,
        UnionSelect,
        TimeBasedFunction,
        FileAccess,
        CommandExecution,
        SystemCatalog,
        EncodedPayload,
        DestructiveStatement,
        TruncatedInput
    };

    /** Human-readable name of a severity. */
    SQLGUARD_API const char* toString(Severity severity) noexcept;

    /** Human-readable name of a signal. */
    SQLGUARD_API const char* toString(Signal signal) noexcept;

    /** Tunable heuristics for the defence-in-depth detector. */
    struct DetectorOptions {
        /** Report SQL comment introducers. */
        bool detectComments = true;

        /** Report a semicolon followed by a destructive keyword. */
        bool detectStackedStatements = true;

        /** Report tautologies such as OR 1=1. */
        bool detectTautologies = true;

        /** Report UNION [ALL] SELECT. */
        bool detectUnionSelect = true;

        /** Report SLEEP, pg_sleep, WAITFOR DELAY and similar. */
        bool detectTimeBasedFunctions = true;

        /** Report file access and command execution primitives. */
        bool detectFileAndCommandAccess = true;

        /** Report well-known system catalog names. */
        bool detectSystemCatalogs = true;

        /** Report common percent-encoded SQL metacharacters. */
        bool detectEncodedPayloads = true;

        /** Report standalone DROP/TRUNCATE/ALTER and similar statements. */
        bool detectDestructiveStatements = true;

        /** Input prefix analysed; larger values are reported as truncated. */
        std::size_t maxInputBytes = 65536;

        /** Minimum overall severity that makes suspicious() true. */
        Severity alertThreshold = Severity::Medium;
    };

    /** One matched suspicious pattern. */
    struct Finding {
        /** Pattern category. */
        Signal signal = Signal::SqlComment;

        /** Static severity assigned to the category. */
        Severity severity = Severity::Low;

        /** Byte offset in the analysed (possibly truncated) input. */
        std::size_t position = 0;

        /** Matched byte length. */
        std::size_t length = 0;

        /** Short copy of the matched text. */
        std::string evidence;

        /** What the pattern indicates. */
        const char* explanation = "";

        /** Suggested defensive action. */
        const char* remediation = "";
    };

    /** Result of a heuristic scan. This is not a security boundary. */
    class SQLGUARD_API DetectionResult {
    public:
        /** True when overall severity reaches the configured threshold. */
        bool suspicious() const noexcept;

        /** Highest aggregate risk level. */
        Severity severity() const noexcept;

        /** Additive risk in [0, 1]. */
        long double score() const noexcept;

        /** True when only a prefix of the input was analysed. */
        bool truncated() const noexcept;

        /** Findings in input order. */
        const std::vector<Finding>& findings() const noexcept;

    private:
        friend class Detector;

        DetectionResult(bool suspicious, Severity severity, long double score, bool truncated, std::vector<Finding> findings);

        bool suspicious_;
        Severity severity_;
        long double score_;
        bool truncated_;
        std::vector<Finding> findings_;
    };

    /** Stateless, bounded heuristic scanner; not a security boundary. */
    class SQLGUARD_API Detector {
    public:
        /** Creates a detector with the supplied options. */
        explicit Detector(DetectorOptions options = DetectorOptions {});

        /** Scans at most options().maxInputBytes bytes. */
        DetectionResult analyze(std::string_view untrustedInput) const;

        /** Returns the immutable configuration. */
        const DetectorOptions& options() const noexcept;

    private:
        DetectorOptions options_;
    };

} // namespace sqlguard

#endif
