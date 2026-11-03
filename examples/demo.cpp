#include "sqlguard/sqlguard.h"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    const std::string attackerText = "' OR '1'='1";

    const sqlguard::StatementOptions options { sqlguard::Dialect::PostgreSql, false };
    sqlguard::Result<sqlguard::Statement> statement = sqlguard::Statement::parse(
        "SELECT id, email FROM users WHERE email = ? AND active = ?", options);
    if(!statement) {
        std::cerr << "parse: " << statement.error().message << '\n';
        return 1;
    }

    const sqlguard::Result<void> emailBinding = statement.value().bind(1, sqlguard::Value::text(attackerText));
    statement.value().bind(2, sqlguard::Value::boolean(true));
    if(!emailBinding) {
        std::cerr << "bind: " << emailBinding.error().message << '\n';
        return 1;
    }

    sqlguard::Result<sqlguard::RenderedQuery> rendered = statement.value().render();
    if(!rendered) {
        std::cerr << "render: " << rendered.error().message << '\n';
        return 1;
    }

    std::cout << "SQL: " << rendered.value().sql() << '\n';
    std::cout << "Bound parameters: " << rendered.value().parameters().size() << '\n';
    std::cout << "First parameter is text: " << (rendered.value().parameters().front().type() == sqlguard::ValueType::Text ? "yes" : "no")
              << '\n';

    const std::vector<std::string> sortableColumns { "created_at", "email", "id" };
    sqlguard::Result<sqlguard::Identifier> orderColumn = sqlguard::Identifier::fromAllowlist("email", sortableColumns,
        sqlguard::Dialect::PostgreSql);
    if(!orderColumn) {
        std::cerr << "identifier: " << orderColumn.error().message << '\n';
        return 1;
    }
    std::cout << "ORDER BY fragment: " << orderColumn.value().quoted() << '\n';

    const sqlguard::Detector detector;
    const sqlguard::DetectionResult detection = detector.analyze(attackerText);
    std::cout << "Detector: " << sqlguard::toString(detection.severity()) << " (score " << static_cast<double>(detection.score()) << ")\n";
    for(const sqlguard::Finding& finding : detection.findings()) {
        std::cout << "  " << sqlguard::toString(finding.signal) << ": " << finding.explanation << '\n';
    }
    return 0;
}
