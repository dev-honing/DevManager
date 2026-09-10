#pragma once
//
// Seam between the Qt UI and the core scanners/managers.
// The UI never calls scanners, QProcess or the filesystem directly
// (design doc s4.3) -- it goes through here.
//
#include <QObject>
#include <QFutureWatcher>
#include <QList>

#include "core/service/service_lifecycle.h"
#include "core/service/service_probe.h"
#include "model.h"

namespace dm {

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);

    bool isScanning() const { return m_watcher.isRunning(); }

public slots:
    void scanEnvironment();        // EnvironmentScanner::scan() off-thread
    void probeServices();          // ServiceProbe::probeAll() off-thread
    void controlService(const QString& id, dm::LifecycleOp op);   // start/stop/restart

signals:
    void scanStarted();
    void scanFinished(const dm::EnvironmentInventory& inventory);
    void servicesProbed(const QList<dm::ServiceState>& services);
    void serviceControlBusy(const QString& id, dm::LifecycleOp op);
    void serviceControlled(const dm::LifecycleResult& result);

private:
    QFutureWatcher<EnvironmentInventory> m_watcher;
    QFutureWatcher<QList<ServiceState>> m_svcWatcher;
    QFutureWatcher<LifecycleResult> m_lifeWatcher;
};

} // namespace dm
