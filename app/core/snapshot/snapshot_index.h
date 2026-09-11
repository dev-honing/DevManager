#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>

namespace dm {

// Read-only listing of existing snapshot folders (backups/<stamp>/manifest.json).
struct SnapshotSummary {
    QString stamp;
    QString path;
    QString capturedAt;
    int componentCount = 0;
    bool consistent = false;
    QString producer;   // "4.1" (PowerShell) etc.
};

class SnapshotIndex {
public:
    // Walks up from `startDir` (max 6 levels) to find a `backups/` directory.
    static QString findBackupsDir(const QString& startDir);

    // Newest first. pre-restore-* / relink-* helper folders are excluded.
    static QList<SnapshotSummary> list(const QString& backupsDir);

    // Reads <snapshotDir>/manifest.json. Empty object + *error set on failure
    // (missing file or unparseable JSON) -- the one place the filename lives.
    static QJsonObject readManifest(const QString& snapshotDir, QString* error = nullptr);
};

} // namespace dm
