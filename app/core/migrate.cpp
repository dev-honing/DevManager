#include "migrate.h"

#include "service/service_probe.h"

namespace dm {

MigrateResult Migrate::run(const QString& bundleFile, const QString& outDir, bool apply)
{
    MigrateResult res;

    const BundleResult unpacked = SnapshotBundle::unpack(bundleFile, outDir);
    if (!unpacked.ok) {
        res.error = unpacked.error;
        return res;
    }
    res.snapshotDir = unpacked.path;

    const auto services = ServiceProbe::probeAll();
    res.preview = RestorePlanner::compute(res.snapshotDir, services);
    if (!res.preview.valid) {
        res.error = res.preview.error;
        return res;
    }

    if (apply) {
        res.restore = RestoreExecutor::run(res.snapshotDir, {});
        res.applied = true;
        res.ok = res.restore.ok;
    } else {
        res.ok = true;   // a valid plan is success for a dry run
    }

    res.health = HealthCheck::run();
    return res;
}

} // namespace dm
