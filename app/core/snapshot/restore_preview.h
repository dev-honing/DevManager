#pragma once
//
// Read-only "what a restore would do" computation from an existing snapshot
// manifest. No files are touched.
//
#include <QList>
#include <QString>
#include <QStringList>

#include "service/service_probe.h"

namespace dm {

struct RestoreTarget {
    QString name;
    QString category;         // core | machine | ...
    QString sourcePath;       // as recorded in the manifest
    QString destPath;         // after remapping to the current user profile
    bool existsNow = false;   // destination currently present -> will be moved aside
    bool included = true;     // machine-category excluded unless opted in
};

struct RestorePreview {
    bool valid = false;
    QString error;

    QString snapshotStamp;
    QString sourceUserProfile;
    QString currentUserProfile;
    int schemaVersion = 0;

    QList<RestoreTarget> targets;
    QStringList linkWarnings;     // linked skills whose target is missing here
    QStringList serviceBlockers;  // running services that block an actual apply
    QString preRestoreBackupDir;  // where current config would be moved
};

class RestorePlanner {
public:
    static RestorePreview compute(const QString& snapshotDir,
                                  const QList<ServiceState>& services = {});
};

} // namespace dm
