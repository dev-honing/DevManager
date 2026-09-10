#pragma once
//
// Start / stop / restart a configured service via its own CLI. This DOES
// change running state (unlike ServiceProbe). Only services whose
// config/scan.json entry has a "lifecycle" block can be controlled.
//
#include <QList>
#include <QString>

#include "config/scan_config.h"

namespace dm {

enum class LifecycleOp { Status, Stop, Start, Restart };

QString lifecycleOpName(LifecycleOp op);

struct LifecycleResult {
    bool ok = false;
    QString serviceId;
    LifecycleOp op = LifecycleOp::Status;
    QString output;
    QString error;
};

class ServiceLifecycle {
public:
    // Blocking (start uses startDetached and returns immediately).
    static LifecycleResult run(const ServiceSpec& spec, LifecycleOp op);
    static LifecycleResult run(const QString& serviceId, LifecycleOp op); // looks up config

    // Stop every currently-running controllable service; returns the ids that
    // were stopped (so the caller can start them again afterwards).
    static QStringList stopRunning(const QList<QString>& runningIds);
};

} // namespace dm
