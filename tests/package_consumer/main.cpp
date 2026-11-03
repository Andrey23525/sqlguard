#include "sqlguard/sqlguard.h"

#include <string>
#include <vector>

int main()
{
    sqlguard::Result<sqlguard::Statement> statement = sqlguard::Statement::parse("SELECT email FROM users WHERE id = ?");
    if(!statement) {
        return 1;
    }
    if(!statement.value().bind(1, sqlguard::Value::integer(7))) {
        return 2;
    }
    sqlguard::Result<sqlguard::RenderedQuery> rendered = statement.value().render();
    if(!rendered) {
        return 3;
    }
    if(rendered.value().parameters().size() != 1) {
        return 4;
    }

    const sqlguard::Result<sqlguard::Identifier> identifier = sqlguard::Identifier::fromAllowlist("email",
        std::vector<std::string> { "email" }, sqlguard::Dialect::PostgreSql);
    if(!identifier) {
        return 5;
    }

    const sqlguard::DetectionResult detection = sqlguard::Detector {}.analyze("' OR '1'='1");
    if(!detection.suspicious()) {
        return 6;
    }
    return 0;
}
