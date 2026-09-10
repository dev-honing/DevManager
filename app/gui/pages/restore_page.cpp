#include "gui/pages/restore_page.h"

#include "core/snapshot/snapshot_index.h"
#include "gui/theme.h"
#include "gui/widgets/tag_badge.h"

#include <QtConcurrent>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dm {

RestorePage::RestorePage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Restore");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("Dry check a snapshot, then apply it. Apply moves your "
                           "current config aside (kept), restores, and rolls back "
                           "on any failure.");
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);

    auto* bar = new QHBoxLayout;
    m_picker = new QComboBox;
    m_picker->setMinimumWidth(260);
    m_dryBtn = new QPushButton("  Run Dry Check");
    m_dryBtn->setObjectName("dryCheckBtn");
    m_dryBtn->setCursor(Qt::PointingHandCursor);
    connect(m_dryBtn, &QPushButton::clicked, this, &RestorePage::runDryCheck);
    m_restoreBtn = new QPushButton("Restore");
    m_restoreBtn->setObjectName("secondaryBtn");
    m_restoreBtn->setEnabled(false);
    m_restoreBtn->setCursor(Qt::PointingHandCursor);
    connect(m_restoreBtn, &QPushButton::clicked, this, &RestorePage::confirmRestore);
    m_recreateLinks = new QCheckBox("recreate links");
    m_recreateLinks->setChecked(true);
    m_includeMachine = new QCheckBox("machine settings");

    bar->addWidget(m_picker);
    bar->addWidget(m_dryBtn);
    bar->addSpacing(12);
    bar->addWidget(m_recreateLinks);
    bar->addWidget(m_includeMachine);
    bar->addStretch(1);
    bar->addWidget(m_restoreBtn);
    lay->addLayout(bar);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    m_progress->hide();
    lay->addWidget(m_progress);

    m_remap = new QLabel;
    m_remap->setFont(monoFont(9));
    m_remap->setStyleSheet(QString("color:%1;").arg(Color::TextSecondary));
    m_remap->hide();
    lay->addWidget(m_remap);

    m_banner = new QLabel;
    m_banner->setWordWrap(true);
    m_banner->setStyleSheet(
        QString("background:rgba(245,158,11,0.12); color:%1; border:1px solid "
                "rgba(245,158,11,0.35); border-radius:8px; padding:8px 12px;")
            .arg(Color::Warning));
    m_banner->hide();
    lay->addWidget(m_banner);

    m_preBackup = new QLabel;
    m_preBackup->setFont(monoFont(8));
    m_preBackup->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    m_preBackup->hide();
    lay->addWidget(m_preBackup);

    m_table = new QTableWidget;
    m_table->setTextElideMode(Qt::ElideMiddle);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(
        {"Component", "Category", "Included", "Destination", "Exists now"});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setDefaultSectionSize(Metric::RowHeight);
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(3, QHeaderView::Stretch);
    hh->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    lay->addWidget(m_table, 1);

    connect(&m_watcher, &QFutureWatcher<RestorePreview>::finished, this,
            [this] { render(m_watcher.result()); });
    connect(&m_execWatcher, &QFutureWatcher<RestoreResult>::finished, this,
            [this] { onRestored(m_execWatcher.result()); });
}

void RestorePage::setContext(const QString& backupsDir,
                             const QList<ServiceState>& services)
{
    m_backupsDir = backupsDir;
    m_services = services;
    reloadSnapshots();
}

void RestorePage::reloadSnapshots()
{
    m_picker->clear();
    for (const SnapshotSummary& s : SnapshotIndex::list(m_backupsDir))
        m_picker->addItem(QString("%1   (%2 components)")
                              .arg(s.stamp)
                              .arg(s.componentCount),
                          s.path);
    m_dryBtn->setEnabled(m_picker->count() > 0);
    if (m_picker->count() == 0)
        m_dryBtn->setText("  No snapshots found");
}

void RestorePage::runDryCheck()
{
    if (m_watcher.isRunning() || m_picker->currentData().isNull())
        return;
    m_dryBtn->setEnabled(false);
    m_dryBtn->setText("  Checking...");
    const QString dir = m_picker->currentData().toString();
    const auto services = m_services;
    m_watcher.setFuture(QtConcurrent::run(
        [dir, services] { return RestorePlanner::compute(dir, services); }));
}

void RestorePage::render(const RestorePreview& pv)
{
    m_dryBtn->setEnabled(true);
    m_dryBtn->setText("  Re-run Dry Check");

    if (!pv.valid) {
        m_banner->setText(pv.error);
        m_banner->show();
        m_table->setRowCount(0);
        return;
    }

    m_remap->setText("path remap:  " + pv.sourceUserProfile + "  →  "
                     + pv.currentUserProfile
                     + (pv.sourceUserProfile.compare(pv.currentUserProfile,
                                                     Qt::CaseInsensitive) == 0
                            ? "   (same machine)"
                            : ""));
    m_remap->show();

    QStringList notes;
    if (!pv.serviceBlockers.isEmpty())
        notes << "Running services block an actual restore: "
                     + pv.serviceBlockers.join(", ") + ".";
    for (const QString& w : pv.linkWarnings)
        notes << "Linked skill — " + w;
    if (!notes.isEmpty()) {
        m_banner->setText(notes.join("\n"));
        m_banner->show();
    } else {
        m_banner->hide();
    }

    m_preBackup->setText("pre-restore backup would go to:  " + pv.preRestoreBackupDir);
    m_preBackup->show();
    m_dryChecked = true;

    m_table->clearContents();
    m_table->setRowCount(pv.targets.size());
    for (int i = 0; i < pv.targets.size(); ++i) {
        const RestoreTarget& t = pv.targets.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(t.name));

        auto* cat = new TagBadge;
        cat->setTag(t.category, t.category == "machine" ? Color::Warning : Color::Primary);
        m_table->setCellWidget(i, 1, TagBadge::cell(cat));

        auto* inc = new QTableWidgetItem(t.included ? "yes" : "skipped");
        inc->setForeground(QColor(t.included ? Color::Success : Color::Muted));
        m_table->setItem(i, 2, inc);

        auto* dst = new QTableWidgetItem(t.destPath);
        dst->setFont(monoFont(9));
        dst->setForeground(QColor(Color::TextSecondary));
        dst->setToolTip("from  " + t.sourcePath + "\nto    " + t.destPath);
        m_table->setItem(i, 3, dst);

        auto* ex = new QTableWidgetItem(t.existsNow ? "move aside" : "new");
        ex->setForeground(QColor(t.existsNow ? Color::Warning : Color::Muted));
        m_table->setItem(i, 4, ex);
    }

    m_restoreBtn->setEnabled(true);
}

void RestorePage::confirmRestore()
{
    if (!m_dryChecked || m_execWatcher.isRunning()
        || m_picker->currentData().isNull())
        return;

    const QString dir = m_picker->currentData().toString();

    const auto proceed = QMessageBox::warning(
        this, "Apply restore",
        QString("This will:\n"
                "  1. move your current config aside (kept under pre-restore-*)\n"
                "  2. restore the snapshot\n"
                "  3. %1recreate links\n"
                "  4. verify, and roll everything back if any step fails\n\n"
                "Snapshot:  %2\n\nContinue?")
            .arg(m_recreateLinks->isChecked() ? "" : "NOT ", dir),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (proceed != QMessageBox::Yes)
        return;

    bool okTyped = false;
    const QString typed = QInputDialog::getText(
        this, "Confirm restore", "Type  RESTORE  to apply:", QLineEdit::Normal,
        QString(), &okTyped);
    if (!okTyped || typed.trimmed() != "RESTORE")
        return;

    RestoreOptions opts;
    opts.includeMachine = m_includeMachine->isChecked();
    opts.recreateLinks = m_recreateLinks->isChecked();

    m_restoreBtn->setEnabled(false);
    m_dryBtn->setEnabled(false);
    m_progress->setFormat("restoring...");
    m_progress->show();

    m_execWatcher.setFuture(QtConcurrent::run([dir, opts, this] {
        return RestoreExecutor::run(
            dir, opts, [this](RestoreState s, const QString& d) {
                const QString label = restoreStateName(s)
                                      + (d.isEmpty() ? QString() : "  " + d);
                QMetaObject::invokeMethod(
                    this, [this, label] { m_progress->setFormat(label); },
                    Qt::QueuedConnection);
            });
    }));
}

void RestorePage::onRestored(const RestoreResult& r)
{
    m_progress->hide();
    m_dryBtn->setEnabled(true);
    m_restoreBtn->setEnabled(true);

    QStringList lines;
    lines << "State: " + restoreStateName(r.state);
    lines << QString("Restored %1 component(s)").arg(r.restored);
    if (!r.preRestoreDir.isEmpty())
        lines << "Previous config kept at:\n  " + r.preRestoreDir;
    if (!r.linkResults.isEmpty())
        lines << "\nLinks:\n  " + r.linkResults.join("\n  ");
    if (!r.errors.isEmpty())
        lines << "\nErrors:\n  " + r.errors.join("\n  ");
    if (!r.rollbackErrors.isEmpty())
        lines << "\nROLLBACK ERRORS:\n  " + r.rollbackErrors.join("\n  ");

    if (r.ok) {
        QMessageBox::information(this, "Restore complete", lines.join("\n"));
        emit restoreApplied();
    } else if (r.state == RestoreState::RolledBack) {
        QMessageBox::warning(this, "Restore failed — rolled back",
                             lines.join("\n"));
    } else {
        QMessageBox::critical(
            this, "Restore failed — rollback incomplete",
            lines.join("\n") + "\n\nManual recovery from:\n  " + r.preRestoreDir);
    }
}

} // namespace dm
