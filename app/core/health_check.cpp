#include "health_check.h"

#include "config/scan_config.h"
#include "path_util.h"
#include "scan/env_scanner.h"
#include "service/service_probe.h"

#include <QFileInfo>

namespace dm {

namespace {
void add(HealthReport& r, const QString& group, const QString& name,
         const QString& status, const QString& detail)
{
    r.items << HealthItem{group, name, status, detail};
    if (status == "fail") r.failCount += 1;
    else if (status == "warn") r.warnCount += 1;
    else r.okCount += 1;
}
} // namespace

HealthReport HealthCheck::run()
{
    HealthReport r;
    const ScanConfig cfg = ScanConfig::load();
    const EnvironmentInventory inv = EnvironmentScanner::scan();

    // configured tools must resolve to an executable
    for (const ToolSpec& t : cfg.tools) {
        const QString path = inv.toolPaths.value(t.id);
        const QString ver = inv.tools.value(t.id);
        if (path.isEmpty())
            add(r, "tool", t.id, "fail", "not found on PATH");
        else
            add(r, "tool", t.id, "ok", ver.isEmpty() ? path : ver);
    }

    // backup roots: absent is fine on a fresh machine, just noteworthy
    for (const QString& raw : cfg.backupRoots) {
        const QString p = path::expand(raw);
        const QString id = QFileInfo(p).fileName();
        if (!p.isEmpty() && QFileInfo::exists(p))
            add(r, "backup-root", id, "ok", p);
        else
            add(r, "backup-root", id.isEmpty() ? raw : id, "warn",
                "absent (restore will create it)");
    }

    // services: running is best, stopped is a warning, error is a failure
    for (const ServiceState& s : ServiceProbe::probeAll()) {
        const QString st = s.level == ServiceState::Running  ? "ok"
                           : s.level == ServiceState::Error  ? "fail"
                                                             : "warn";
        add(r, "service", s.name.isEmpty() ? s.id : s.name, st, s.detail);
    }

    r.ok = (r.failCount == 0);
    return r;
}

} // namespace dm
