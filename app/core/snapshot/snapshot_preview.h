#pragma once
//
// Read-only "what a snapshot would contain" computation. Walks the configured
// backup roots, classifies each top-level entry against a built-in policy
// table and sizes it. Copies nothing, changes nothing.
//
#include <QList>
#include <QString>
#include <QStringList>

#include "service/service_probe.h"

namespace dm {

enum class SnapshotPolicy { Backup, InventoryOnly, Regenerate, Exclude };

QString policyName(SnapshotPolicy p);

struct PlannedArtifact {
    QString rootId;          // ".claude", ".codex", ...
    QString name;            // top-level entry name
    QString path;            // absolute
    SnapshotPolicy policy = SnapshotPolicy::Exclude;
    QString reason;
    qint64 sizeBytes = 0;
    int fileCount = 0;
    bool isLink = false;
    QString linkTarget;
};

struct SnapshotPreview {
    bool valid = false;
    QStringList missingRoots;
    QList<PlannedArtifact> artifacts;

    qint64 backupBytes = 0;
    int backupFiles = 0;
    int inventoryOnlyCount = 0;
    int regenerateCount = 0;
    int excludeCount = 0;

    QStringList serviceBlockers;   // services that should be stopped first
};

class SnapshotPlanner {
public:
    // `services` is used only to report blockers; pass {} if unknown.
    static SnapshotPreview compute(const QList<ServiceState>& services = {});
};

} // namespace dm
