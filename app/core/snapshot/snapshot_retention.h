#pragma once
//
// Retention: decide which snapshots under backups/ are old enough to drop.
// plan() is read-only and always keeps at least the newest snapshot; apply()
// is the only destructive call and removes exactly what plan() listed.
//
#include <QList>
#include <QString>
#include <QStringList>

namespace dm {

struct RetentionPolicy {
    int keepLast = 0;   // keep the N newest snapshots (0 = criterion off)
    int keepDays = 0;   // keep snapshots captured within D days   (0 = off)
    // A snapshot is kept if it satisfies EITHER criterion; pruned only if it
    // fails both. With both 0, nothing is pruned.
};

struct PrunePlan {
    QString backupsDir;
    QStringList keep;    // stamps kept
    QStringList prune;   // absolute snapshot dirs to remove
    qint64 pruneBytes = 0;
};

struct PruneResult {
    bool ok = false;
    int removed = 0;
    qint64 bytes = 0;
    QStringList errors;
};

class SnapshotRetention {
public:
    static PrunePlan plan(const QString& backupsDir, const RetentionPolicy& policy);
    static PruneResult apply(const PrunePlan& plan);
};

} // namespace dm
