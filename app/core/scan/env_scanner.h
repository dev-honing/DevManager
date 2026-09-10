#pragma once
#include "model.h"

namespace dm {

// Orchestrates the read-only Phase 1 scan and returns a fully populated
// EnvironmentInventory. No file writes, no service control.
class EnvironmentScanner {
public:
    static EnvironmentInventory scan();

    // sub-scans, exposed for targeted testing / reuse
    static QMap<QString, QString> scanTools();
    static QString scanQt();
    static QString scanVisualStudio();
    static WslInfo scanWsl();
    static QList<Package> scanGlobalPackages();
    static QMap<QString, QString> scanEnv();

    static QString maskSecret(const QString& value);
    static bool isSecretName(const QString& name);
};

} // namespace dm
