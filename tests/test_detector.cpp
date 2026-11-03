#include "sqlguard/detector.h"

#include <string>

#include "sqlguard/dialect.h"
#include "test_framework.h"

namespace detector_tests {

    bool containsSignal(const sqlguard::DetectionResult& result, sqlguard::Signal signal)
    {
        for(const sqlguard::Finding& finding : result.findings()) {
            if(finding.signal == signal) {
                return true;
            }
        }
        return false;
    }

} // namespace detector_tests

using detector_tests::containsSignal;

SG_TEST(detectorLeavesBenignTextAlone)
{
    const sqlguard::Detector detector;
    const sqlguard::DetectionResult name = detector.analyze("O'Reilly");
    SG_CHECK(!name.suspicious());
    SG_CHECK(name.severity() == sqlguard::Severity::None);
    SG_CHECK(name.findings().empty());

    const sqlguard::DetectionResult email = detector.analyze("user@example.com");
    SG_CHECK(!email.suspicious());
    SG_CHECK(email.findings().empty());
}

SG_TEST(detectorFindsTautologies)
{
    const sqlguard::Detector detector;
    const char* payloads[] = { "' OR '1'='1", "1 OR 1=1", "x' OR 'a' LIKE 'a", "1 AND 2=2", "admin' OR '1'='1' --" };
    for(const char* payload : payloads) {
        const sqlguard::DetectionResult result = detector.analyze(payload);
        SG_CHECK(result.suspicious());
        SG_CHECK(containsSignal(result, sqlguard::Signal::Tautology));
        SG_CHECK(result.severity() != sqlguard::Severity::None);
    }
}

SG_TEST(detectorFindsUnionSelect)
{
    const sqlguard::Detector detector;
    const sqlguard::DetectionResult simple = detector.analyze("1 UNION SELECT password FROM users");
    SG_CHECK(simple.suspicious());
    SG_CHECK(containsSignal(simple, sqlguard::Signal::UnionSelect));
    SG_CHECK(simple.severity() == sqlguard::Severity::High);

    const sqlguard::DetectionResult withAll = detector.analyze("1 UNION ALL SELECT password FROM users");
    SG_CHECK(containsSignal(withAll, sqlguard::Signal::UnionSelect));
}

SG_TEST(detectorFindsStackedStatementAndComment)
{
    const sqlguard::Detector detector;
    const sqlguard::DetectionResult result = detector.analyze("'; DROP TABLE users; --");
    SG_CHECK(result.suspicious());
    SG_CHECK(result.severity() == sqlguard::Severity::Critical);
    SG_CHECK(containsSignal(result, sqlguard::Signal::StackedStatement));
    SG_CHECK(containsSignal(result, sqlguard::Signal::DestructiveStatement));
    SG_CHECK(containsSignal(result, sqlguard::Signal::SqlComment));
}

SG_TEST(detectorFindsTimeBasedPrimitives)
{
    const sqlguard::Detector detector;
    const char* payloads[] = { "SLEEP(5)", "pg_sleep(5)", "BENCHMARK(1000000, MD5('x'))", "WAITFOR DELAY '00:00:05'" };
    for(const char* payload : payloads) {
        const sqlguard::DetectionResult result = detector.analyze(payload);
        SG_CHECK(result.suspicious());
        SG_CHECK(containsSignal(result, sqlguard::Signal::TimeBasedFunction));
    }
}

SG_TEST(detectorFindsFileAndCommandPrimitives)
{
    const sqlguard::Detector detector;
    const sqlguard::DetectionResult file = detector.analyze("LOAD_FILE('/etc/passwd')");
    SG_CHECK(file.suspicious());
    SG_CHECK(containsSignal(file, sqlguard::Signal::FileAccess));

    const sqlguard::DetectionResult command = detector.analyze("EXEC xp_cmdshell('dir')");
    SG_CHECK(command.suspicious());
    SG_CHECK(containsSignal(command, sqlguard::Signal::CommandExecution));
    SG_CHECK(command.severity() == sqlguard::Severity::Critical);

    const sqlguard::DetectionResult noParentheses = detector.analyze("EXEC master..xp_cmdshell 'dir'");
    SG_CHECK(noParentheses.suspicious());
    SG_CHECK(containsSignal(noParentheses, sqlguard::Signal::CommandExecution));
}

SG_TEST(detectorFindsSystemCatalogAndEncoding)
{
    const sqlguard::Detector detector;
    const sqlguard::DetectionResult catalog = detector.analyze("SELECT * FROM information_schema.tables");
    SG_CHECK(catalog.suspicious());
    SG_CHECK(containsSignal(catalog, sqlguard::Signal::SystemCatalog));

    const sqlguard::DetectionResult encoded = detector.analyze("%27%20OR%201%3D1");
    SG_CHECK(containsSignal(encoded, sqlguard::Signal::EncodedPayload));
    SG_CHECK(encoded.severity() == sqlguard::Severity::Low);
    SG_CHECK(!encoded.suspicious());
}

SG_TEST(detectorHonoursOptions)
{
    sqlguard::DetectorOptions options;
    options.detectTautologies = false;
    const sqlguard::Detector quiet(options);
    const sqlguard::DetectionResult result = quiet.analyze("' OR '1'='1");
    SG_CHECK(!result.suspicious());
    SG_CHECK(result.findings().empty());

    sqlguard::DetectorOptions strictOptions;
    strictOptions.alertThreshold = sqlguard::Severity::High;
    const sqlguard::Detector strict(strictOptions);
    const sqlguard::DetectionResult medium = strict.analyze("' OR '1'='1");
    SG_CHECK(medium.severity() == sqlguard::Severity::Medium);
    SG_CHECK(!medium.suspicious());
}

SG_TEST(detectorReportsTruncation)
{
    sqlguard::DetectorOptions options;
    options.maxInputBytes = 3;
    const sqlguard::Detector detector(options);
    const sqlguard::DetectionResult result = detector.analyze("SLEEP(5)");
    SG_CHECK(result.truncated());
    SG_CHECK(containsSignal(result, sqlguard::Signal::TruncatedInput));
    SG_CHECK(!result.suspicious());
}

SG_TEST(detectorDoesNotTreatCommentsAsSuspiciousAlone)
{
    const sqlguard::Detector detector;
    const sqlguard::DetectionResult result = detector.analyze("plain text -- with a dash comment");
    SG_CHECK(!result.suspicious());
    SG_CHECK(result.severity() == sqlguard::Severity::Low);
    SG_CHECK(containsSignal(result, sqlguard::Signal::SqlComment));
}

SG_TEST(detectorNamesAreStable)
{
    SG_CHECK_EQ(std::string(sqlguard::toString(sqlguard::Severity::High)), std::string("high"));
    SG_CHECK_EQ(std::string(sqlguard::toString(sqlguard::Signal::UnionSelect)), std::string("union_select"));
    SG_CHECK_EQ(std::string(sqlguard::toString(sqlguard::Dialect::PostgreSql)), std::string("postgresql"));
}
