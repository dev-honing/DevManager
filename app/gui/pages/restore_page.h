#pragma once
#include <QWidget>
#include <QFutureWatcher>

#include "core/snapshot/restore_executor.h"
#include "core/snapshot/restore_preview.h"

class QComboBox;
class QTableWidget;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;

namespace dm {

class RestorePage : public QWidget {
    Q_OBJECT
public:
    explicit RestorePage(QWidget* parent = nullptr);
    void setContext(const QString& backupsDir, const QList<ServiceState>& services);

signals:
    void restoreApplied();   // -> app should rescan

private:
    void reloadSnapshots();
    void runDryCheck();
    void render(const RestorePreview& pv);
    void confirmRestore();
    void onRestored(const RestoreResult& r);

    QString m_backupsDir;
    QList<ServiceState> m_services;
    bool m_dryChecked = false;

    QFutureWatcher<RestorePreview> m_watcher;
    QFutureWatcher<RestoreResult> m_execWatcher;

    QComboBox* m_picker = nullptr;
    QPushButton* m_dryBtn = nullptr;
    QPushButton* m_restoreBtn = nullptr;
    QCheckBox* m_includeMachine = nullptr;
    QCheckBox* m_recreateLinks = nullptr;
    QLabel* m_remap = nullptr;
    QLabel* m_banner = nullptr;
    QLabel* m_preBackup = nullptr;
    QProgressBar* m_progress = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace dm
