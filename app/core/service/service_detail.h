#pragma once
//
// "Running" alone conflates three different questions: is the process alive,
// is it healthy, and is the current environment actually wired to use it.
// ServiceProbe only answers the first (cheap, TCP-only, safe to call on every
// scan). This answers the other two, on demand -- it shells out per service,
// so the caller decides when that's worth it rather than paying for it on
// every refresh.
//
#include <QList>
#include <QMap>
#include <QString>

#include "service/service_probe.h"

namespace dm {

struct ServiceDetail {
    QString id;
    QString name;
    bool running = false;
    QString health;    // "healthy" | "unhealthy" | "" (no signal from this service's CLI)
    bool envWired = false;   // envIndicatorVar's current value names this service's own port
    QString envVar;          // which var was checked, "" if none configured
};

class ServiceDetailCheck {
public:
    // `services` from ServiceProbe::probeAll() (for the running/stopped flag);
    // `env` from EnvironmentInventory::env (already-scanned, masked env vars).
    static QList<ServiceDetail> run(const QList<ServiceState>& services,
                                    const QMap<QString, QString>& env);
};

} // namespace dm
