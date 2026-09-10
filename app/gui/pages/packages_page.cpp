#include "gui/pages/packages_page.h"

#include "gui/theme.h"
#include "gui/widgets/segmented_control.h"
#include "gui/widgets/tag_badge.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dm {

PackagesPage::PackagesPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Packages");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("Global npm / pip packages. Inventory only — never auto-installed.");
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);

    auto* bar = new QHBoxLayout;
    m_filter = new SegmentedControl;
    m_filter->addSegment("all", "All");   // manager segments are added per-scan
    connect(m_filter, &SegmentedControl::changed, this, [this] { rebuild(); });
    m_search = new QLineEdit;
    m_search->setPlaceholderText("Filter by name...");
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(280);
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuild(); });
    m_count = new QLabel;
    m_count->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    bar->addWidget(m_filter);
    bar->addStretch(1);
    bar->addWidget(m_count);
    bar->addWidget(m_search);
    lay->addLayout(bar);

    m_table = new QTableWidget;
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Name", "Manager", "Version", "Restore policy"});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setDefaultSectionSize(Metric::RowHeight);
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Stretch);
    hh->setSectionResizeMode(1, QHeaderView::Fixed);
    hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 80);
    m_table->setColumnWidth(3, 150);
    lay->addWidget(m_table, 1);
}

void PackagesPage::setInventory(const EnvironmentInventory& inv)
{
    m_inv = inv;

    QStringList managers;
    for (const auto& p : inv.globalPackages)
        if (!managers.contains(p.manager))
            managers << p.manager;
    managers.sort();

    const QString prev = m_filter->current();
    m_filter->clear();
    m_filter->addSegment("all", "All");
    for (const QString& m : managers)
        m_filter->addSegment(m, m);
    m_filter->setCurrent((prev.isEmpty() || !managers.contains(prev)) ? "all" : prev);
    rebuild();
}

void PackagesPage::setSearchFilter(const QString& text)
{
    m_search->setText(text);
}

void PackagesPage::rebuild()
{
    const QString mgr = m_filter->current().isEmpty() ? "all" : m_filter->current();
    const QString q = m_search->text();

    QList<Package> rows;
    for (const auto& p : m_inv.globalPackages) {
        if (mgr != "all" && p.manager != mgr)
            continue;
        if (!q.isEmpty() && !p.name.contains(q, Qt::CaseInsensitive))
            continue;
        rows << p;
    }

    m_table->clearContents();
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const Package& p = rows.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(p.name));
        auto* mgr = new TagBadge;
        mgr->setTag(p.manager, p.manager == "npm" ? "#cb3837" : "#3776ab");
        m_table->setCellWidget(i, 1, TagBadge::cell(mgr));
        auto* ver = new QTableWidgetItem(p.version);
        ver->setFont(monoFont(9));
        m_table->setItem(i, 2, ver);
        auto* pol = new TagBadge;
        pol->setTag(p.restorePolicy == "inventory-only" ? "Inventory only" : p.restorePolicy,
                    Color::TextSecondary);
        m_table->setCellWidget(i, 3, TagBadge::cell(pol));
    }
    m_count->setText(QString("%1 of %2").arg(rows.size()).arg(m_inv.globalPackages.size()));
}

} // namespace dm
