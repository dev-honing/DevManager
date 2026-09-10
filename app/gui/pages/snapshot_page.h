#pragma once
#include <QWidget>
#include <QFutureWatcher>

#include "core/snapshot/snapshot_coordinator.h"
#include "core/snapshot/snapshot_executor.h"
#include "core/snapshot/snapshot_preview.h"

class QTableWidget;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;

namespace dm {

class SnapshotPage : public QWidget {
    Q_OBJECT
public:
    explicit SnapshotPage(QWidget* parent = nullptr);
    void setServices(const QList<ServiceState>& services);
    void setBackupsDir(const QString& dir);

signals:
    void snapshotCreated();   // so the app can refresh Recent Snapshots

private:
    void runDryCheck();
    void render(const SnapshotPreview& pv);
    void createSnapshot();
    void onCreated(const SnapshotResult& r);
    static QString humanBytes(qint64 b);

    QList<ServiceState> m_services;
    QString m_backupsDir;
    SnapshotPreview m_preview;
    QStringList m_serviceNotes;   // stop/restart notes from the last consistent run

    QFutureWatcher<SnapshotPreview> m_watcher;
    QFutureWatcher<SnapshotResult> m_execWatcher;

    QPushButton* m_dryBtn = nullptr;
    QPushButton* m_createBtn = nullptr;
    QCheckBox* m_stopFirst = nullptr;   // consistent snapshot: stop blockers first
    QLabel* m_summary = nullptr;
    QLabel* m_blockers = nullptr;
    QProgressBar* m_progress = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace dm
