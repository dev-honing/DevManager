#include "bootstrap.h"

#include "config/scan_config.h"
#include "health_check.h"

namespace dm {

BootstrapPlan Bootstrap::plan()
{
    BootstrapPlan bp;
    const ScanConfig cfg = ScanConfig::load();
    const HealthReport h = HealthCheck::run();

    for (const HealthItem& it : h.items) {
        if (it.group != "tool" || it.status != "fail")
            continue;
        BootstrapStep step;
        step.tool = it.name;
        for (const ToolSpec& t : cfg.tools) {
            if (t.id == it.name) {
                step.command = t.install;
                break;
            }
        }
        step.haveHint = !step.command.isEmpty();
        bp.missing += 1;
        if (step.haveHint)
            bp.withHint += 1;
        bp.steps << step;
    }
    return bp;
}

} // namespace dm
