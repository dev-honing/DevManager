#include "gui/pages/snapshot_page.h"

#include "gui/theme.h"
#include "gui/widgets/table_factory.h"
#include "gui/widgets/tag_badge.h"

#include <QtConcurrent>

#include <QDir>
#include <QFileInfo>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "json_io.h"
#include "scan/env_scanner.h"

namespace dm {

QString SnapshotPage::humanBytes(qint64 b)
{
    const char* u[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(b);
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return QString::number(v, 'f', v < 10 && i > 0 ? 1 : 0) + " " + u[i];
}

static const char* policyColor(SnapshotPolicy p)
{
    switch (p) {
    case SnapshotPolicy::Backup: return Color::Success;
    case SnapshotPolicy::InventoryOnly: return Color::Primary;
    case SnapshotPolicy::Regenerate: return Color::Warning;
    case SnapshotPolicy::Exclude: return Color::Muted;
    }
    return Color::Muted;
}

SnapshotPage::SnapshotPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Snapshots");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("A dry check computes what a snapshot would include. "
                           "Nothing is written.");
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);

    auto* bar = new QHBoxLayout;
    m_dryBtn = new QPushButton("  Run Dry Check");
    m_dryBtn->setObjectName("dryCheckBtn");
    m_dryBtn->setCursor(Qt::PointingHandCursor);
    connect(m_dryBtn, &QPushButton::clicked, this, &SnapshotPage::runDryCheck);

    m_createBtn = new QPushButton("Create Snapshot");
    m_createBtn->setObjectName("secondaryBtn");
    m_createBtn->setEnabled(false);
    m_createBtn->setToolTip("Run a dry check first");
    m_createBtn->setCursor(Qt::PointingHandCursor);
    connect(m_createBtn, &QPushButton::clicked, this, &SnapshotPage::createSnapshot);

    m_stopFirst = new QCheckBox("stop running services first");
    m_stopFirst->setToolTip("Stop the services holding backup files open, take a "
                            "consistent snapshot, then start them again.");
    m_stopFirst->hide();

    m_summary = new QLabel("Run a dry check to see the plan.");
    m_summary->setStyleSheet(QString("color:%1;").arg(Color::Muted));

    bar->addWidget(m_dryBtn);
    bar->addWidget(m_createBtn);
    bar->addWidget(m_stopFirst);
    bar->addStretch(1);
    bar->addWidget(m_summary);
    lay->addLayout(bar);

    m_progress = new QProgressBar;
    m_progress->setTextVisible(true);
    m_progress->hide();
    lay->addWidget(m_progress);

    m_blockers = new QLabel;
    m_blockers->setWordWrap(true);
    m_blockers->setStyleSheet(
        QString("background:rgba(245,158,11,0.12); color:%1; border:1px solid "
                "rgba(245,158,11,0.35); border-radius:8px; padding:8px 12px;")
            .arg(Color::Warning));
    m_blockers->hide();
    lay->addWidget(m_blockers);

    m_table = makeListTable({"Root", "Entry", "Policy", "Size", "Files"});
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::Stretch);
    hh->setSectionResizeMode(2, QHeaderView::Fixed);
    hh->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(2, 128);
    lay->addWidget(m_table, 1);

    connect(&m_watcher, &QFutureWatcher<SnapshotPreview>::finished, this,
            [this] { render(m_watcher.result()); });
    connect(&m_execWatcher, &QFutureWatcher<SnapshotResult>::finished, this,
            [this] { onCreated(m_execWatcher.result()); });
}

void SnapshotPage::setServices(const QList<ServiceState>& services)
{
    m_services = services;
}

void SnapshotPage::setBackupsDir(const QString& dir) { m_backupsDir = dir; }

void SnapshotPage::runDryCheck()
{
    if (m_watcher.isRunning())
        return;
    m_dryBtn->setEnabled(false);
    m_dryBtn->setText("  Checking...");
    m_summary->setText("walking backup roots (read-only)...");
    const auto services = m_services;
    m_watcher.setFuture(QtConcurrent::run(
        [services] { return SnapshotPlanner::compute(services); }));
}

void SnapshotPage::render(const SnapshotPreview& pv)
{
    m_preview = pv;
    m_dryBtn->setEnabled(true);
    m_dryBtn->setText("  Re-run Dry Check");
    m_createBtn->setEnabled(pv.valid && pv.backupFiles > 0 && !m_backupsDir.isEmpty());
    m_createBtn->setToolTip(m_backupsDir.isEmpty()
                                ? "No backups/ directory located"
                                : "Write a snapshot to " + m_backupsDir);

    if (!pv.serviceBlockers.isEmpty()) {
        m_blockers->setText("Running services: " + pv.serviceBlockers.join(", ")
                            + ".  Stop them for a migration-grade (consistent) snapshot.");
        m_blockers->show();
        m_stopFirst->show();
    } else {
        m_blockers->hide();
        m_stopFirst->hide();
    }

    m_summary->setText(
        QString("backup %1 (%2 files)   ·   inventory-only %3   ·   regenerate %4   "
                "·   excluded %5")
            .arg(humanBytes(pv.backupBytes))
            .arg(pv.backupFiles)
            .arg(pv.inventoryOnlyCount)
            .arg(pv.regenerateCount)
            .arg(pv.excludeCount));

    m_table->clearContents();
    m_table->setRowCount(pv.artifacts.size());
    for (int i = 0; i < pv.artifacts.size(); ++i) {
        const PlannedArtifact& a = pv.artifacts.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(a.rootId));
        const QString entryText =
            a.name + (a.isLink ? "  → " + a.linkTarget : QString());
        auto* entry = new QTableWidgetItem(entryText);
        entry->setToolTip(entryText + "\n" + a.path + "\n(" + a.reason + ")");
        m_table->setItem(i, 1, entry);

        auto* badge = new TagBadge;
        badge->setTag(policyName(a.policy), policyColor(a.policy));
        m_table->setCellWidget(i, 2, TagBadge::cell(badge));

        auto* size = new QTableWidgetItem(humanBytes(a.sizeBytes));
        size->setFont(monoFont(9));
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 3, size);
        auto* files = new QTableWidgetItem(QString::number(a.fileCount));
        files->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        files->setForeground(QColor(Color::Muted));
        m_table->setItem(i, 4, files);
    }
}

void SnapshotPage::createSnapshot()
{
    if (m_execWatcher.isRunning() || !m_preview.valid || m_backupsDir.isEmpty())
        return;

    const bool live = !m_preview.serviceBlockers.isEmpty();
    const bool stopFirst = live && m_stopFirst->isChecked();
    QString text = QString("Write a snapshot under\n%1\n\n"
                           "Reads your configured dirs; writes only there. "
                           "Nothing else is touched.")
                       .arg(m_backupsDir);
    if (stopFirst)
        text += QString("\n\n%1 will be stopped, the snapshot taken, then "
                        "started again.")
                    .arg(m_preview.serviceBlockers.join(" / "));
    else if (live)
        text += QString("\n\n%1 running — the snapshot will be marked "
                        "non-consistent (live).")
                    .arg(m_preview.serviceBlockers.join(" / "));

    if (QMessageBox::question(this, "Create Snapshot", text,
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;

    m_createBtn->setEnabled(false);
    m_dryBtn->setEnabled(false);
    m_progress->setRange(0, 0);          // busy until first progress tick
    m_progress->setFormat("preparing...");
    m_progress->show();

    const SnapshotPreview pv = m_preview;
    const QString dest = m_backupsDir;
    const auto services = m_services;
    m_serviceNotes.clear();

    // build a fresh inventory json to store alongside the manifest
    const QJsonObject inv = EnvironmentScanner::scan().toJson();

    m_execWatcher.setFuture(QtConcurrent::run([pv, dest, inv, services, stopFirst, this] {
        auto progress = [this](int done, int total, const QString& label) {
            QMetaObject::invokeMethod(
                this,
                [this, done, total, label] {
                    if (total > 0) {
                        m_progress->setRange(0, total);
                        m_progress->setValue(done);
                    }
                    m_progress->setFormat(
                        QString("%1  (%2/%3)").arg(label).arg(done).arg(total));
                },
                Qt::QueuedConnection);
        };

        if (stopFirst) {
            const ConsistentSnapshotResult cr =
                SnapshotCoordinator::runConsistent(services, dest, inv, progress);
            QStringList notes;
            if (!cr.stopped.isEmpty())
                notes << "stopped " + cr.stopped.join(", ");
            if (!cr.restarted.isEmpty())
                notes << "restarted " + cr.restarted.join(", ");
            notes += cr.serviceErrors;
            QMetaObject::invokeMethod(
                this, [this, notes] { m_serviceNotes = notes; },
                Qt::QueuedConnection);
            return cr.snapshot;
        }
        return SnapshotExecutor::run(pv, dest, inv, progress);
    }));
}

void SnapshotPage::onCreated(const SnapshotResult& r)
{
    m_progress->hide();
    m_dryBtn->setEnabled(true);
    m_createBtn->setEnabled(true);

    if (r.ok) {
        m_summary->setText(QString("snapshot written: %1  ·  %2 items, %3")
                               .arg(QFileInfo(r.snapshotDir).fileName())
                               .arg(r.copied)
                               .arg(humanBytes(r.bytes)));
        QString detail = "Snapshot: " + r.snapshotDir;
        if (!m_serviceNotes.isEmpty())
            detail = "Services: " + m_serviceNotes.join("; ") + "\n\n" + detail;
        if (!r.linkNotes.isEmpty())
            detail += "\n\nLinks recorded (not copied):\n  "
                      + r.linkNotes.mid(0, 12).join("\n  ");
        QMessageBox::information(this, "Snapshot created", detail);
        emit snapshotCreated();
    } else {
        QMessageBox::warning(
            this, "Snapshot failed",
            QString("%1 of %2 items failed.\n\n%3")
                .arg(r.failed)
                .arg(r.copied + r.failed)
                .arg(r.errors.mid(0, 10).join("\n")));
    }
}

} // namespace dm
