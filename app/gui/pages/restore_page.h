#pragma once
#include <QWidget>
#include <QFutureWatcher>

#include "core/snapshot/restore_preview.h"

class QComboBox;
class QTableWidget;
class QLabel;
class QPushButton;

namespace dm {

class RestorePage : public QWidget {
    Q_OBJECT
public:
    explicit RestorePage(QWidget* parent = nullptr);
    void setContext(const QString& backupsDir, const QList<ServiceState>& services);

private:
    void reloadSnapshots();
    void runDryCheck();
    void render(const RestorePreview& pv);
    void confirmRestore();

    QString m_backupsDir;
    QList<ServiceState> m_services;
    QFutureWatcher<RestorePreview> m_watcher;

    QComboBox* m_picker = nullptr;
    QPushButton* m_dryBtn = nullptr;
    QPushButton* m_restoreBtn = nullptr;
    QLabel* m_remap = nullptr;
    QLabel* m_banner = nullptr;
    QLabel* m_preBackup = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace dm
