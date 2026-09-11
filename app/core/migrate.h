#pragma once
//
// End-to-end "new PC" orchestrator: unbundle a snapshot archive, plan (or
// apply) its restore, then report readiness. Ties together SnapshotBundle,
// RestorePlanner/RestoreExecutor and HealthCheck -- each already usable on
// its own via the CLI; this is just the one-command path through all three.
//
#include <QString>

#include "health_check.h"
#include "snapshot/restore_executor.h"
#include "snapshot/restore_preview.h"
#include "snapshot/snapshot_bundle.h"

namespace dm {

struct MigrateResult {
    bool ok = false;
    QString error;          // set only if unbundle itself failed
    QString snapshotDir;    // where the bundle was extracted
    RestorePreview preview; // always computed (read-only)
    bool applied = false;   // true if RestoreExecutor actually ran
    RestoreResult restore;  // meaningful only when applied
    HealthReport health;    // taken after restore (or after unbundle in dry mode)
};

class Migrate {
public:
    // outDir: where to extract the bundle (created if missing). apply=false
    // (the default call site) only unbundles + plans + reports health.
    static MigrateResult run(const QString& bundleFile, const QString& outDir, bool apply);
};

} // namespace dm
