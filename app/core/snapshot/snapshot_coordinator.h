#pragma once
//
// Consistent snapshot: stop the running services that hold backup-root files
// open (ServiceSpec.snapshotBlocker), take the snapshot, then start them again.
// A plain SnapshotExecutor run on a live machine yields consistentSnapshot=false;
// going through here clears the blockers first so the manifest is consistent.
//
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "service/service_probe.h"
#include "snapshot/snapshot_executor.h"

namespace dm {

struct ConsistentSnapshotResult {
    SnapshotResult snapshot;
    QStringList stopped;          // blocker ids stopped before the snapshot
    QStringList restarted;        // stopped ids brought back afterwards
    QStringList serviceErrors;    // stop/start failures (human-readable)
    bool servicesRestored = true; // every stopped service was started again
};

class SnapshotCoordinator {
public:
    // `services` is the already-probed liveness list (pass ServiceProbe::probeAll()).
    static ConsistentSnapshotResult runConsistent(
        const QList<ServiceState>& services,
        const QString& destBaseDir,
        const QJsonObject& inventoryJson = {},
        SnapshotExecutor::Progress progress = {});
};

} // namespace dm
