#pragma once
//
// Seam between the Qt UI and the core scanners/managers.
// The UI never calls scanners, QProcess or the filesystem directly
// (design doc s4.3) -- it goes through here.
//
#include <QObject>
#include <QFutureWatcher>

#include "model.h"

namespace dm {

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);

    bool isScanning() const { return m_watcher.isRunning(); }

public slots:
    // Runs EnvironmentScanner::scan() off the GUI thread.
    void scanEnvironment();

signals:
    void scanStarted();
    void scanFinished(const dm::EnvironmentInventory& inventory);

private:
    QFutureWatcher<EnvironmentInventory> m_watcher;
};

} // namespace dm
