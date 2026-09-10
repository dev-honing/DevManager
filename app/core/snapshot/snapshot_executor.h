#pragma once
//
// Creates a snapshot on disk from a SnapshotPreview. Reads the source dirs,
// writes ONLY under <destBase>/<stamp>/. Reparse points are recorded as link
// metadata, never dereferenced/copied (avoids the PowerShell restore bug).
//
#include <functional>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "snapshot/snapshot_preview.h"

namespace dm {

struct SnapshotResult {
    bool ok = false;
    QString snapshotDir;
    int copied = 0;
    int failed = 0;
    qint64 bytes = 0;
    QStringList errors;
    QStringList linkNotes;   // reparse points recorded, not copied
};

class SnapshotExecutor {
public:
    // progress(done, total, label). Called from the calling thread.
    using Progress = std::function<void(int, int, const QString&)>;

    static SnapshotResult run(const SnapshotPreview& preview,
                              const QString& destBaseDir,
                              const QJsonObject& inventoryJson = {},
                              Progress progress = {});
};

} // namespace dm
