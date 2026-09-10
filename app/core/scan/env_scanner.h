#pragma once
#include "config/scan_config.h"
#include "model.h"

namespace dm {

// Orchestrates the read-only Phase 1 scan and returns a fully populated
// EnvironmentInventory. No file writes, no service control.
// What it looks for (tools, env patterns, package managers, Qt locations)
// comes from ScanConfig, not from code.
class EnvironmentScanner {
public:
    static EnvironmentInventory scan();

    // sub-scans, exposed for targeted testing / reuse
    struct ToolScan {
        QMap<QString, QString> versions;
        QMap<QString, QString> paths;
        QMap<QString, QString> categories;
    };
    static ToolScan scanTools(const ScanConfig& cfg);
    static QString scanQt(const ScanConfig& cfg);
    static QString scanVisualStudio();
    static WslInfo scanWsl();
    static QList<Package> scanGlobalPackages(const ScanConfig& cfg);
    static QMap<QString, QString> scanEnv(const ScanConfig& cfg);

    static QString maskSecret(const QString& value);
    static bool isSecretName(const QString& name, const QStringList& patterns);
};

} // namespace dm
