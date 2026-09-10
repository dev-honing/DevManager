#include "gui/app_controller.h"

#include <QtConcurrent>

#include "scan/env_scanner.h"

namespace dm {

AppController::AppController(QObject* parent) : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<EnvironmentInventory>::finished, this, [this] {
        emit scanFinished(m_watcher.result());
    });
}

void AppController::scanEnvironment()
{
    if (m_watcher.isRunning())
        return;
    emit scanStarted();
    m_watcher.setFuture(QtConcurrent::run([] { return EnvironmentScanner::scan(); }));
}

} // namespace dm
