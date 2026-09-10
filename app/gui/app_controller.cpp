#include "gui/app_controller.h"

#include <QtConcurrent>

#include "scan/env_scanner.h"

namespace dm {

AppController::AppController(QObject* parent) : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<EnvironmentInventory>::finished, this, [this] {
        emit scanFinished(m_watcher.result());
    });
    connect(&m_svcWatcher, &QFutureWatcher<QList<ServiceState>>::finished, this, [this] {
        emit servicesProbed(m_svcWatcher.result());
    });
    connect(&m_lifeWatcher, &QFutureWatcher<LifecycleResult>::finished, this, [this] {
        emit serviceControlled(m_lifeWatcher.result());
        probeServices();   // refresh the badges after a start/stop
    });
}

void AppController::controlService(const QString& id, LifecycleOp op)
{
    if (m_lifeWatcher.isRunning())
        return;
    emit serviceControlBusy(id, op);
    m_lifeWatcher.setFuture(QtConcurrent::run(
        [id, op] { return ServiceLifecycle::run(id, op); }));
}

void AppController::scanEnvironment()
{
    if (m_watcher.isRunning())
        return;
    emit scanStarted();
    m_watcher.setFuture(QtConcurrent::run([] { return EnvironmentScanner::scan(); }));
    probeServices();
}

void AppController::probeServices()
{
    if (m_svcWatcher.isRunning())
        return;
    m_svcWatcher.setFuture(QtConcurrent::run([] { return ServiceProbe::probeAll(); }));
}

} // namespace dm
