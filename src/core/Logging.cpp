#include "core/Logging.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCosmic, "cosmic")

namespace Logging {

void init(bool debug)
{
    // Default level is INFO: warnings/errors always show up, and INFO must
    // be enabled explicitly (Qt's category default only enables warning+).
    // --debug additionally enables the "cosmic" debug category.
    if (debug) {
        QLoggingCategory::setFilterRules(QStringLiteral("nebula.info=true\n"
                                                        "nebula.debug=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral("nebula.info=true\n"
                                                        "nebula.debug=false"));
    }
}

} // namespace Logging
