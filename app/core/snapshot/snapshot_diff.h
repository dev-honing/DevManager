#pragma once
//
// Compare two snapshot manifests by component: what was added, removed or
// changed between them. Read-only (reads the two manifest.json files only).
// Uses the recorded sha256 where present, else size, else link target.
//
#include <QList>
#include <QString>

namespace dm {

struct SnapshotDelta {
    QString name;
    QString change;    // "added" | "removed" | "changed"
    QString note;      // short human reason (sha/size/target/policy diff)
};

struct DiffResult {
    bool ok = false;
    QString error;
    QString stampA, stampB;
    int added = 0;
    int removed = 0;
    int changed = 0;
    QList<SnapshotDelta> deltas;   // added + removed + changed, never "same"
};

class SnapshotDiff {
public:
    static DiffResult compare(const QString& snapshotDirA,
                              const QString& snapshotDirB);
};

} // namespace dm
