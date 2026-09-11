#pragma once
#include <QWidget>
#include <QFutureWatcher>

#include "core/bootstrap.h"
#include "core/health_check.h"
#include "core/snapshot/snapshot_retention.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QPlainTextEdit;
class QProgressBar;

namespace dm {

// Settings/Maintenance: new-PC readiness (Health), install hints (Bootstrap),
// and snapshot retention (Prune) -- the CLI-only P9/P10 features, in the GUI.
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);
    void setBackupsDir(const QString& dir);

signals:
    void snapshotsPruned();   // -> app should refresh Recent Snapshots

private:
    // health
    void runHealth();
    void renderHealth(const HealthReport& r);
    // bootstrap
    void runBootstrap();
    void renderBootstrap(const BootstrapPlan& p);
    // retention
    void previewPrune();
    void renderPrune(const PrunePlan& p);
    void confirmAndApplyPrune();
    void onPruned(const PruneResult& r);

    QString m_backupsDir;
    PrunePlan m_lastPlan;

    QFutureWatcher<HealthReport> m_healthWatcher;
    QFutureWatcher<BootstrapPlan> m_bootstrapWatcher;
    QFutureWatcher<PrunePlan> m_pruneWatcher;
    QFutureWatcher<PruneResult> m_pruneApplyWatcher;

    QComboBox* m_profileCombo = nullptr;
    QPushButton* m_healthBtn = nullptr;
    QLabel* m_healthSummary = nullptr;
    QTableWidget* m_healthTable = nullptr;

    QPushButton* m_bootstrapBtn = nullptr;
    QPlainTextEdit* m_bootstrapText = nullptr;
    QPushButton* m_copyBtn = nullptr;

    QSpinBox* m_keepLast = nullptr;
    QSpinBox* m_keepDays = nullptr;
    QPushButton* m_previewPruneBtn = nullptr;
    QPushButton* m_applyPruneBtn = nullptr;
    QLabel* m_pruneSummary = nullptr;
    QTableWidget* m_pruneTable = nullptr;
    QProgressBar* m_pruneProgress = nullptr;
};

} // namespace dm
