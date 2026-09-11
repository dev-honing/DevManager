#include "service/service_detail.h"

#include "config/scan_config.h"
#include "service/service_lifecycle.h"

#include <QRegularExpression>

namespace dm {

QList<ServiceDetail> ServiceDetailCheck::run(const QList<ServiceState>& services,
                                             const QMap<QString, QString>& env)
{
    QList<ServiceDetail> out;
    const ScanConfig cfg = ScanConfig::load();

    for (const ServiceSpec& spec : cfg.services) {
        ServiceDetail d;
        d.id = spec.id;
        d.name = spec.id;
        for (const ServiceState& s : services)
            if (s.id == spec.id) {
                d.name = s.name;
                d.running = (s.level == ServiceState::Running);
                break;
            }

        if (!spec.lifecycle.statusArgs.isEmpty()) {
            const LifecycleResult r = ServiceLifecycle::run(spec, LifecycleOp::Status);
            static const QRegularExpression healthy(
                "Healthy:\\s*(yes|no)", QRegularExpression::CaseInsensitiveOption);
            const auto m = healthy.match(r.output);
            if (m.hasMatch())
                d.health = m.captured(1).toLower() == "yes" ? "healthy" : "unhealthy";
        }

        if (!spec.envIndicatorVar.isEmpty() && spec.port > 0) {
            d.envVar = spec.envIndicatorVar;
            d.envWired = env.value(spec.envIndicatorVar)
                             .contains(":" + QString::number(spec.port));
        }

        out << d;
    }
    return out;
}

} // namespace dm
