#pragma once
//
// Applies a snapshot back onto the machine. Destructive: it moves the current
// config aside (pre-restore-<stamp>/), copies the snapshot in, recreates
// linked skills, verifies, and rolls everything back on any failure.
//
// Safety invariants:
//   - current config is moved (not deleted) to a kept pre-restore folder
//   - reparse points are never recursed into (see fs_ops)
//   - a link target that is missing yields a report, never a broken link
//   - any failure triggers a full rollback in reverse order
//
#include <functional>

#include <QList>
#include <QString>
#include <QStringList>

namespace dm {

enum class RestoreState {
    Planned, Preserving, Restoring, Relinking, Verifying, Done,
    RollingBack, RolledBack, Failed
};
QString restoreStateName(RestoreState s);

struct RestoreOptions {
    bool includeMachine = false;   // restore docker/wsl settings too
    bool recreateLinks = true;     // rebuild junctions for linked skills
};

struct RestoreStep {
    QString name;
    QString destPath;
    QString preservedPath;   // where the pre-existing item was moved
    bool restored = false;
};

struct RestoreResult {
    RestoreState state = RestoreState::Planned;
    bool ok = false;
    QString snapshotDir;
    QString preRestoreDir;
    int restored = 0;
    QList<RestoreStep> steps;
    QStringList linkResults;    // human-readable per linked skill
    QStringList errors;
    QStringList rollbackErrors;
};

class RestoreExecutor {
public:
    using Progress = std::function<void(RestoreState, const QString&)>;
    static RestoreResult run(const QString& snapshotDir,
                             const RestoreOptions& opts,
                             Progress progress = {});
};

} // namespace dm
