#include "gui/pages/settings_page.h"

#include "gui/theme.h"
#include "gui/widgets/status_badge.h"
#include "gui/widgets/table_factory.h"

#include <QtConcurrent>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/config/scan_config.h"

namespace dm {

namespace {

QLabel* sectionTitle(const QString& text)
{
    auto* l = new QLabel(text);
    l->setFont(uiFont(13, QFont::DemiBold, true));
    return l;
}

QLabel* sectionSub(const QString& text)
{
    auto* l = new QLabel(text);
    l->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    return l;
}

StatusBadge::Level badgeLevel(const QString& status)
{
    if (status == "ok") return StatusBadge::Ok;
    if (status == "warn") return StatusBadge::Warn;
    if (status == "fail" || status == "mismatch" || status == "missing")
        return StatusBadge::Error;
    return StatusBadge::Neutral;
}

QWidget* badgeCell(const QString& status)
{
    auto* w = new QWidget;
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(8, 0, 8, 0);
    auto* b = new StatusBadge;
    b->set(badgeLevel(status), status);
    l->addWidget(b);
    l->addStretch(1);
    return w;
}

// size a table to fit `rowCount` rows without an inner scrollbar, clamped to
// [minH, maxH] so a long list still scrolls instead of pushing the rest of
// the page off-screen.
void fitTableHeight(QTableWidget* t, int rowCount, int minH, int maxH)
{
    const int header = t->horizontalHeader()->height();
    const int wanted = header + qMax(rowCount, 1) * Metric::RowHeight + 6;
    t->setFixedHeight(qBound(minH, wanted, maxH));
}

} // namespace

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Settings");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    lay->addWidget(title);
    lay->addWidget(sectionSub("New-PC readiness, install hints, and snapshot housekeeping."));

    // ---------------------------------------------------------- Health
    lay->addWidget(sectionTitle("Environment Health"));
    auto* healthBar = new QHBoxLayout;
    m_profileCombo = new QComboBox;
    m_profileCombo->addItem("All tools", QString());
    for (const QString& p : ScanConfig::load().hostProfiles.keys())
        m_profileCombo->addItem(p, p);
    m_healthBtn = new QPushButton("  Run Health Check");
    m_healthBtn->setCursor(Qt::PointingHandCursor);
    connect(m_healthBtn, &QPushButton::clicked, this, &SettingsPage::runHealth);
    m_healthSummary = new QLabel("Not run yet.");
    m_healthSummary->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    healthBar->addWidget(new QLabel("Profile:"));
    healthBar->addWidget(m_profileCombo);
    healthBar->addWidget(m_healthBtn);
    healthBar->addStretch(1);
    healthBar->addWidget(m_healthSummary);
    lay->addLayout(healthBar);

    m_healthTable = makeListTable({"Group", "Name", "Status", "Detail"});
    m_healthTable->setColumnWidth(0, 100);
    m_healthTable->setColumnWidth(1, 140);
    m_healthTable->setColumnWidth(2, 100);
    lay->addWidget(m_healthTable);

    // ---------------------------------------------------- Service details
    lay->addWidget(sectionTitle("Service Details"));
    lay->addWidget(sectionSub("\"Running\" just means the process is alive -- not that it's "
                              "healthy, or that anything is actually routed through it."));
    auto* svcBar = new QHBoxLayout;
    m_serviceDetailBtn = new QPushButton("  Check Service Details");
    m_serviceDetailBtn->setCursor(Qt::PointingHandCursor);
    connect(m_serviceDetailBtn, &QPushButton::clicked, this, &SettingsPage::checkServiceDetails);
    m_serviceDetailHint = new QLabel;
    m_serviceDetailHint->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    svcBar->addWidget(m_serviceDetailBtn);
    svcBar->addStretch(1);
    svcBar->addWidget(m_serviceDetailHint);
    lay->addLayout(svcBar);

    m_serviceDetailTable = new QTableWidget;
    m_serviceDetailTable->setColumnCount(4);
    m_serviceDetailTable->setHorizontalHeaderLabels({"Service", "Running", "Health", "Env wired"});
    m_serviceDetailTable->verticalHeader()->setVisible(false);
    m_serviceDetailTable->setShowGrid(false);
    m_serviceDetailTable->setAlternatingRowColors(true);
    m_serviceDetailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_serviceDetailTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_serviceDetailTable->setFocusPolicy(Qt::NoFocus);
    m_serviceDetailTable->verticalHeader()->setDefaultSectionSize(Metric::RowHeight);
    m_serviceDetailTable->horizontalHeader()->setStretchLastSection(true);
    lay->addWidget(m_serviceDetailTable);

    // ---------------------------------------------------------- Bootstrap
    lay->addWidget(sectionTitle("Bootstrap — install missing tools"));
    auto* bsBar = new QHBoxLayout;
    m_bootstrapBtn = new QPushButton("  Show Install Commands");
    m_bootstrapBtn->setCursor(Qt::PointingHandCursor);
    connect(m_bootstrapBtn, &QPushButton::clicked, this, &SettingsPage::runBootstrap);
    m_copyBtn = new QPushButton("Copy All");
    m_copyBtn->setObjectName("secondaryBtn");
    m_copyBtn->setEnabled(false);
    connect(m_copyBtn, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_bootstrapText->toPlainText()); });
    bsBar->addWidget(m_bootstrapBtn);
    bsBar->addWidget(m_copyBtn);
    bsBar->addStretch(1);
    lay->addLayout(bsBar);

    m_bootstrapText = new QPlainTextEdit;
    m_bootstrapText->setReadOnly(true);
    m_bootstrapText->setFont(monoFont(9));
    m_bootstrapText->setPlaceholderText("Run a health check (or click above) to see install commands for missing tools. Nothing here is ever executed.");
    m_bootstrapText->setMaximumHeight(120);
    lay->addWidget(m_bootstrapText);

    // ---------------------------------------------------------- Retention
    lay->addWidget(sectionTitle("Snapshot Retention"));
    auto* retBar = new QHBoxLayout;
    m_keepLast = new QSpinBox;
    m_keepLast->setRange(0, 999);
    m_keepLast->setValue(10);
    m_keepLast->setSpecialValueText("off");
    m_keepDays = new QSpinBox;
    m_keepDays->setRange(0, 3650);
    m_keepDays->setValue(0);
    m_keepDays->setSpecialValueText("off");
    m_previewPruneBtn = new QPushButton("  Preview");
    m_previewPruneBtn->setObjectName("dryCheckBtn");
    m_previewPruneBtn->setCursor(Qt::PointingHandCursor);
    connect(m_previewPruneBtn, &QPushButton::clicked, this, &SettingsPage::previewPrune);
    m_applyPruneBtn = new QPushButton("Delete old snapshots");
    m_applyPruneBtn->setObjectName("secondaryBtn");
    m_applyPruneBtn->setEnabled(false);
    m_applyPruneBtn->setToolTip("Run Preview first");
    connect(m_applyPruneBtn, &QPushButton::clicked, this, &SettingsPage::confirmAndApplyPrune);
    m_pruneSummary = new QLabel("Keep the newest N snapshots and/or those newer than D days.");
    m_pruneSummary->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    retBar->addWidget(new QLabel("keep last"));
    retBar->addWidget(m_keepLast);
    retBar->addWidget(new QLabel("keep days"));
    retBar->addWidget(m_keepDays);
    retBar->addWidget(m_previewPruneBtn);
    retBar->addWidget(m_applyPruneBtn);
    retBar->addStretch(1);
    lay->addLayout(retBar);
    lay->addWidget(m_pruneSummary);

    m_pruneProgress = new QProgressBar;
    m_pruneProgress->hide();
    lay->addWidget(m_pruneProgress);

    m_pruneTable = makeListTable({"Stamp", "Action"});
    lay->addWidget(m_pruneTable, 1);

    connect(&m_healthWatcher, &QFutureWatcher<HealthReport>::finished, this,
            [this] { renderHealth(m_healthWatcher.result()); });
    connect(&m_bootstrapWatcher, &QFutureWatcher<BootstrapPlan>::finished, this,
            [this] { renderBootstrap(m_bootstrapWatcher.result()); });
    connect(&m_pruneWatcher, &QFutureWatcher<PrunePlan>::finished, this,
            [this] { renderPrune(m_pruneWatcher.result()); });
    connect(&m_pruneApplyWatcher, &QFutureWatcher<PruneResult>::finished, this,
            [this] { onPruned(m_pruneApplyWatcher.result()); });
    connect(&m_serviceDetailWatcher, &QFutureWatcher<QList<ServiceDetail>>::finished, this,
            [this] { renderServiceDetails(m_serviceDetailWatcher.result()); });
}

void SettingsPage::setBackupsDir(const QString& dir) { m_backupsDir = dir; }

void SettingsPage::setServiceContext(const QList<ServiceState>& services,
                                     const QMap<QString, QString>& env)
{
    m_services = services;
    m_env = env;
}

// -------------------------------------------------------------- Health
void SettingsPage::runHealth()
{
    if (m_healthWatcher.isRunning())
        return;
    m_healthBtn->setEnabled(false);
    m_healthBtn->setText("  Checking...");
    const QString profile = m_profileCombo->currentData().toString();
    m_healthWatcher.setFuture(
        QtConcurrent::run([profile] { return HealthCheck::run(profile); }));
}

void SettingsPage::renderHealth(const HealthReport& r)
{
    m_healthBtn->setEnabled(true);
    m_healthBtn->setText("  Run Health Check");
    m_healthSummary->setText(
        QString("%1  ·  %2 ok, %3 warn, %4 fail")
            .arg(r.ok ? "READY" : "NOT READY")
            .arg(r.okCount).arg(r.warnCount).arg(r.failCount));
    m_healthSummary->setStyleSheet(
        QString("color:%1; font-weight:600;").arg(r.ok ? Color::Success : Color::Warning));

    m_healthTable->clearContents();
    m_healthTable->setRowCount(r.items.size());
    for (int i = 0; i < r.items.size(); ++i) {
        const HealthItem& it = r.items.at(i);
        m_healthTable->setItem(i, 0, new QTableWidgetItem(it.group));
        m_healthTable->setItem(i, 1, new QTableWidgetItem(it.name));
        m_healthTable->setCellWidget(i, 2, badgeCell(it.status));
        auto* detail = new QTableWidgetItem(it.detail);
        detail->setForeground(QColor(Color::Muted));
        m_healthTable->setItem(i, 3, detail);
    }
    fitTableHeight(m_healthTable, r.items.size(), 120, 400);
}

// -------------------------------------------------------------- Bootstrap
void SettingsPage::runBootstrap()
{
    if (m_bootstrapWatcher.isRunning())
        return;
    m_bootstrapBtn->setEnabled(false);
    m_bootstrapBtn->setText("  Checking...");
    m_bootstrapWatcher.setFuture(QtConcurrent::run([] { return Bootstrap::plan(); }));
}

void SettingsPage::renderBootstrap(const BootstrapPlan& p)
{
    m_bootstrapBtn->setEnabled(true);
    m_bootstrapBtn->setText("  Show Install Commands");

    if (p.missing == 0) {
        m_bootstrapText->setPlainText("Nothing missing.");
        m_copyBtn->setEnabled(false);
        return;
    }
    QStringList lines;
    lines << QString("# %1 missing, %2 with an install hint -- review before running")
                 .arg(p.missing).arg(p.withHint);
    for (const BootstrapStep& s : p.steps)
        lines << (s.haveHint ? s.command
                             : QString("# %1: no install hint -- install manually").arg(s.tool));
    m_bootstrapText->setPlainText(lines.join("\n"));
    m_copyBtn->setEnabled(true);
}

// -------------------------------------------------------------- Retention
void SettingsPage::previewPrune()
{
    if (m_pruneWatcher.isRunning() || m_backupsDir.isEmpty())
        return;
    RetentionPolicy pol;
    pol.keepLast = m_keepLast->value();
    pol.keepDays = m_keepDays->value();
    if (pol.keepLast <= 0 && pol.keepDays <= 0) {
        m_pruneSummary->setText("Set \"keep last\" and/or \"keep days\" above zero first.");
        return;
    }
    m_previewPruneBtn->setEnabled(false);
    const QString dir = m_backupsDir;
    m_pruneWatcher.setFuture(
        QtConcurrent::run([dir, pol] { return SnapshotRetention::plan(dir, pol); }));
}

void SettingsPage::renderPrune(const PrunePlan& p)
{
    m_previewPruneBtn->setEnabled(true);
    m_lastPlan = p;
    m_applyPruneBtn->setEnabled(!p.prune.isEmpty());
    m_applyPruneBtn->setToolTip(p.prune.isEmpty() ? "Nothing to prune"
                                                  : "Permanently deletes the listed snapshots");

    const double mb = p.pruneBytes / 1024.0 / 1024.0;
    m_pruneSummary->setText(QString("%1 kept, %2 to prune (%3 MB)")
                                .arg(p.keep.size()).arg(p.prune.size())
                                .arg(mb, 0, 'f', mb < 10 ? 1 : 0));

    m_pruneTable->clearContents();
    m_pruneTable->setRowCount(p.keep.size() + p.prune.size());
    int row = 0;
    for (const QString& k : p.keep) {
        m_pruneTable->setItem(row, 0, new QTableWidgetItem(k));
        m_pruneTable->setCellWidget(row, 1, badgeCell("ok"));
        ++row;
    }
    for (const QString& dir : p.prune) {
        m_pruneTable->setItem(row, 0, new QTableWidgetItem(QFileInfo(dir).fileName()));
        m_pruneTable->setCellWidget(row, 1, badgeCell("fail"));
        ++row;
    }
}

void SettingsPage::confirmAndApplyPrune()
{
    if (m_lastPlan.prune.isEmpty() || m_pruneApplyWatcher.isRunning())
        return;
    bool okTyped = false;
    const QString typed = QInputDialog::getText(
        this, "Delete old snapshots",
        QString("This permanently deletes %1 snapshot(s) under backups/.\n\n"
                "Type  PRUNE  to confirm:")
            .arg(m_lastPlan.prune.size()),
        QLineEdit::Normal, {}, &okTyped);
    if (!okTyped || typed.trimmed() != "PRUNE")
        return;

    m_applyPruneBtn->setEnabled(false);
    m_previewPruneBtn->setEnabled(false);
    m_pruneProgress->setRange(0, 0);
    m_pruneProgress->show();
    const PrunePlan plan = m_lastPlan;
    m_pruneApplyWatcher.setFuture(
        QtConcurrent::run([plan] { return SnapshotRetention::apply(plan); }));
}

void SettingsPage::onPruned(const PruneResult& r)
{
    m_pruneProgress->hide();
    m_previewPruneBtn->setEnabled(true);
    m_pruneSummary->setText(QString("removed %1 (%2 MB)%3")
                                .arg(r.removed)
                                .arg(r.bytes / 1024.0 / 1024.0, 0, 'f', 1)
                                .arg(r.ok ? "" : "  -- " + r.errors.join("; ")));
    if (!r.ok)
        QMessageBox::warning(this, "Prune failed", r.errors.join("\n"));
    emit snapshotsPruned();
    previewPrune();   // refresh the table against the now-smaller backups/
}

// -------------------------------------------------------------- Service detail
void SettingsPage::checkServiceDetails()
{
    if (m_serviceDetailWatcher.isRunning())
        return;
    m_serviceDetailBtn->setEnabled(false);
    m_serviceDetailBtn->setText("  Checking...");
    const auto services = m_services;
    const auto env = m_env;
    m_serviceDetailWatcher.setFuture(
        QtConcurrent::run([services, env] { return ServiceDetailCheck::run(services, env); }));
}

void SettingsPage::renderServiceDetails(const QList<ServiceDetail>& details)
{
    m_serviceDetailBtn->setEnabled(true);
    m_serviceDetailBtn->setText("  Check Service Details");
    m_serviceDetailHint->setText(QString("checked %1 service(s)").arg(details.size()));

    m_serviceDetailTable->clearContents();
    m_serviceDetailTable->setRowCount(details.size());
    for (int i = 0; i < details.size(); ++i) {
        const ServiceDetail& d = details.at(i);
        m_serviceDetailTable->setItem(i, 0, new QTableWidgetItem(d.name));
        m_serviceDetailTable->setCellWidget(i, 1, badgeCell(d.running ? "ok" : "warn"));

        const QString health = d.health.isEmpty() ? "n/a" : d.health;
        auto* h = new QTableWidgetItem(health);
        h->setForeground(QColor(d.health == "unhealthy" ? Color::Danger
                                : d.health == "healthy" ? Color::Success
                                                        : Color::Muted));
        m_serviceDetailTable->setItem(i, 2, h);

        QString wired = "n/a";
        if (!d.envVar.isEmpty())
            wired = d.envWired ? "wired (" + d.envVar + ")" : "not wired (" + d.envVar + ")";
        auto* w = new QTableWidgetItem(wired);
        w->setForeground(QColor(d.envVar.isEmpty() ? Color::Muted
                                : d.envWired ? Color::Success
                                            : Color::Warning));
        m_serviceDetailTable->setItem(i, 3, w);
    }

    fitTableHeight(m_serviceDetailTable, details.size(), 120, 280);
}

} // namespace dm
