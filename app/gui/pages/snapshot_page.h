#pragma once
#include <QWidget>
#include <QFutureWatcher>

#include "core/snapshot/snapshot_preview.h"

class QTableWidget;
class QLabel;
class QPushButton;

namespace dm {

class SnapshotPage : public QWidget {
    Q_OBJECT
public:
    explicit SnapshotPage(QWidget* parent = nullptr);
    void setServices(const QList<ServiceState>& services);

private:
    void runDryCheck();
    void render(const SnapshotPreview& pv);
    static QString humanBytes(qint64 b);

    QList<ServiceState> m_services;
    QFutureWatcher<SnapshotPreview> m_watcher;

    QPushButton* m_dryBtn = nullptr;
    QPushButton* m_createBtn = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_blockers = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace dm
