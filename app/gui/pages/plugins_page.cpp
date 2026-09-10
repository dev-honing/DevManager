#include "gui/pages/plugins_page.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"
#include "gui/widgets/tag_badge.h"

#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dm {

PluginsPage::PluginsPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Plugins");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("Discovered agent plugins across all plugin roots.");
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);

    m_stack = new QStackedWidget;
    lay->addWidget(m_stack, 1);

    // 0: empty state
    auto* empty = new QWidget;
    auto* el = new QVBoxLayout(empty);
    el->setAlignment(Qt::AlignCenter);
    el->setSpacing(6);
    auto* ic = new QLabel;
    ic->setPixmap(icons::pixmap("plugins", QColor(Color::Border), 40));
    ic->setAlignment(Qt::AlignCenter);
    auto* t = new QLabel("No plugins discovered.");
    t->setFont(uiFont(12, QFont::DemiBold));
    t->setAlignment(Qt::AlignCenter);
    m_emptyHint = new QLabel("Plugin roots were scanned successfully.");
    m_emptyHint->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    el->addWidget(ic);
    el->addWidget(t);
    el->addWidget(m_emptyHint);
    m_stack->addWidget(empty);

    // 1: table
    m_table = new QTableWidget;
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Name", "Host", "Type", "Path"});
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
    m_stack->addWidget(m_table);
}

void PluginsPage::setInventory(const EnvironmentInventory& inv)
{
    int rows = 0;
    for (const auto& p : inv.plugins)
        rows += p.locations.size();

    if (rows == 0) {
        m_emptyHint->setText(
            QString("%1 plugin root(s) were scanned successfully.")
                .arg(inv.pluginRoots.size()));
        m_stack->setCurrentIndex(0);
        return;
    }

    m_table->clearContents();
    m_table->setRowCount(rows);
    int r = 0;
    for (const auto& p : inv.plugins) {
        for (const auto& l : p.locations) {
            auto* n = new QTableWidgetItem(p.name);
            n->setFont(uiFont(10, QFont::DemiBold));
            m_table->setItem(r, 0, n);
            m_table->setItem(r, 1, new QTableWidgetItem(l.host));
            auto* badge = new TagBadge;
            badge->setTag(l.link.isLink ? "Linked" : "User",
                          l.link.isLink ? Color::Purple : Color::Success);
            m_table->setCellWidget(r, 2, TagBadge::cell(badge));
            auto* path = new QTableWidgetItem(l.path);
            path->setFont(monoFont(9));
            path->setForeground(QColor(Color::TextSecondary));
            m_table->setItem(r, 3, path);
            ++r;
        }
    }
    m_stack->setCurrentIndex(1);
}

} // namespace dm
