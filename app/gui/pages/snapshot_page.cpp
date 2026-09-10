#include "gui/pages/snapshot_page.h"

#include "gui/theme.h"
#include "gui/widgets/tag_badge.h"

#include <QtConcurrent>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

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
    m_createBtn->setToolTip("Snapshot execution is not implemented yet — dry check only.");

    m_summary = new QLabel("Run a dry check to see the plan.");
    m_summary->setStyleSheet(QString("color:%1;").arg(Color::Muted));

    bar->addWidget(m_dryBtn);
    bar->addWidget(m_createBtn);
    bar->addStretch(1);
    bar->addWidget(m_summary);
    lay->addLayout(bar);

    m_blockers = new QLabel;
    m_blockers->setWordWrap(true);
    m_blockers->setStyleSheet(
        QString("background:rgba(245,158,11,0.12); color:%1; border:1px solid "
                "rgba(245,158,11,0.35); border-radius:8px; padding:8px 12px;")
            .arg(Color::Warning));
    m_blockers->hide();
    lay->addWidget(m_blockers);

    m_table = new QTableWidget;
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Root", "Entry", "Policy", "Size", "Files"});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setDefaultSectionSize(Metric::RowHeight);
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
}

void SnapshotPage::setServices(const QList<ServiceState>& services)
{
    m_services = services;
}

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
    m_dryBtn->setEnabled(true);
    m_dryBtn->setText("  Re-run Dry Check");

    if (!pv.serviceBlockers.isEmpty()) {
        m_blockers->setText("Running services: " + pv.serviceBlockers.join(", ")
                            + ".  Stop them for a migration-grade (consistent) snapshot.");
        m_blockers->show();
    } else {
        m_blockers->hide();
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
        auto* entry = new QTableWidgetItem(
            a.name + (a.isLink ? "  → " + a.linkTarget : QString()));
        entry->setToolTip(a.reason);
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

} // namespace dm
