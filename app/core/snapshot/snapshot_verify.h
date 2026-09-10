#pragma once
//
// Re-hash a snapshot's backed-up components and compare against the sha256
// recorded in its manifest. Read-only: opens files, changes nothing. Use it
// before trusting a snapshot for a restore.
//
#include <QList>
#include <QString>

namespace dm {

struct ComponentCheck {
    QString name;
    QString backupPath;
    QString status;   // "ok" | "mismatch" | "missing" | "no-digest"
};

struct VerifyResult {
    bool ok = false;          // manifest read AND no mismatch/missing
    QString error;            // set when the manifest itself is unusable
    QString snapshotDir;
    int okCount = 0;
    int badCount = 0;         // mismatch + missing
    int skippedCount = 0;     // links / components with no recorded digest
    QList<ComponentCheck> components;
};

class SnapshotVerify {
public:
    static VerifyResult check(const QString& snapshotDir);
};

} // namespace dm
