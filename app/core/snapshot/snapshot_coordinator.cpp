#include "snapshot/snapshot_coordinator.h"

#include "config/scan_config.h"
#include "service/service_lifecycle.h"
#include "snapshot/snapshot_preview.h"

namespace dm {

ConsistentSnapshotResult SnapshotCoordinator::runConsistent(
    const QList<ServiceState>& services, const QString& destBaseDir,
    const QJsonObject& inventoryJson, SnapshotExecutor::Progress progress)
{
    ConsistentSnapshotResult res;
    const ScanConfig cfg = ScanConfig::load();

    QStringList blockerIds;
    for (const ServiceSpec& s : cfg.services)
        if (s.snapshotBlocker && s.lifecycle.canControl())
            blockerIds << s.id;

    // stop the running blockers
    for (const ServiceState& s : services) {
        if (s.level != ServiceState::Running || !blockerIds.contains(s.id))
            continue;
        const LifecycleResult r = ServiceLifecycle::run(s.id, LifecycleOp::Stop);
        if (r.ok)
            res.stopped << s.id;
        else
            res.serviceErrors << ("stop " + s.id + ": "
                                  + (r.error.isEmpty() ? r.output : r.error));
    }

    // with the blockers down, recompute the plan so no blocker is reported
    QList<ServiceState> remaining;
    for (const ServiceState& s : services)
        if (!res.stopped.contains(s.id))
            remaining << s;
    const SnapshotPreview pv = SnapshotPlanner::compute(remaining);
    res.snapshot = SnapshotExecutor::run(pv, destBaseDir, inventoryJson, progress);

    // start them again, newest-stopped first
    for (int i = res.stopped.size() - 1; i >= 0; --i) {
        const QString id = res.stopped.at(i);
        const LifecycleResult r = ServiceLifecycle::run(id, LifecycleOp::Start);
        if (r.ok)
            res.restarted << id;
        else
            res.serviceErrors << ("start " + id + ": "
                                  + (r.error.isEmpty() ? r.output : r.error));
    }
    res.servicesRestored = (res.restarted.size() == res.stopped.size());
    return res;
}

} // namespace dm
