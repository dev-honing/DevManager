#pragma once
#include <QWidget>
#include <QList>

#include "core/service/service_probe.h"
#include "core/snapshot/snapshot_index.h"

class QVBoxLayout;

namespace dm {

class RightPanel : public QWidget {
    Q_OBJECT
public:
    explicit RightPanel(QWidget* parent = nullptr);

    void setServices(const QList<ServiceState>& services);
    void setSnapshots(const QList<SnapshotSummary>& snapshots);

signals:
    void scanRequested();
    void navigateTo(const QString& pageId);
    void openDevFolderRequested();

private:
    QWidget* section(const QString& title, QWidget* body);

    QVBoxLayout* m_statusRows = nullptr;
    QVBoxLayout* m_snapshotRows = nullptr;
};

} // namespace dm
