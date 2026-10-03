#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcCosmic)

namespace Logging {

/// Initializes logging. Debug output is only enabled when \p debug is true
/// (from the --debug CLI flag) or when the "nebula.*" logging rules say so.
void init(bool debug);

} // namespace Logging
