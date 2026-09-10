#include "gui/pages/skills_page.h"

#include "gui/theme.h"
#include "gui/widgets/segmented_control.h"
#include "gui/widgets/tag_badge.h"

#include <QHeaderView>
#include <QLabel>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

namespace dm {

QString SkillsPage::skillType(const NamedItem& s)
{
    bool anyLinked = false, allSystem = !s.locations.isEmpty();
    for (const auto& l : s.locations) {
        if (l.link.isLink)
            anyLinked = true;
        if (l.classification != "system")
            allSystem = false;
    }
    if (anyLinked)
        return "linked";
    if (allSystem)
        return "system";
    return "user";
}

SkillsPage::SkillsPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Skills");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("Discovered agent skills and their link topology.");
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);

    m_filter = new SegmentedControl;
    for (const char* f : {"all", "user", "linked", "system"}) {
        QString label = f;
        label[0] = label[0].toUpper();
        m_filter->addSegment(f, label);
    }
    connect(m_filter, &SegmentedControl::changed, this, [this] { rebuild(); });
    lay->addWidget(m_filter);

    m_table = new QTableWidget;
    m_table->setTextElideMode(Qt::ElideMiddle);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Name", "Type", "Hosts", "Locations"});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setDefaultSectionSize(Metric::RowHeight + 2);
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::Fixed);
    hh->setSectionResizeMode(2, QHeaderView::Stretch);
    hh->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(1, 96);
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row) { showDetail(row); });
    lay->addWidget(m_table, 1);

    m_detail = new QFrame;
    m_detail->setObjectName("panelCard");
    m_detail->setAttribute(Qt::WA_StyledBackground, true);
    m_detailBody = new QVBoxLayout(m_detail);
    m_detailBody->setContentsMargins(14, 12, 14, 12);
    m_detailBody->setSpacing(4);
    m_detailTitle = new QLabel;
    m_detailTitle->setFont(uiFont(11, QFont::DemiBold));
    m_detailBody->addWidget(m_detailTitle);
    m_detail->hide();
    lay->addWidget(m_detail);
}

void SkillsPage::setInventory(const EnvironmentInventory& inv)
{
    m_inv = inv;
    rebuild();
}

void SkillsPage::setSearchFilter(const QString& text)
{
    m_search = text;
    rebuild();
}

void SkillsPage::rebuild()
{
    const QString filt = m_filter->current().isEmpty() ? "all" : m_filter->current();
    m_table->clearContents();
    m_rowToSkill.clear();
    m_detail->hide();

    int row = 0;
    for (int i = 0; i < m_inv.skills.size(); ++i) {
        const NamedItem& s = m_inv.skills.at(i);
        const QString type = skillType(s);
        if (filt != "all" && type != filt)
            continue;
        if (!m_search.isEmpty() && !s.name.contains(m_search, Qt::CaseInsensitive))
            continue;

        QSet<QString> hosts;
        for (const auto& l : s.locations)
            hosts.insert(l.host);
        QStringList hostList(hosts.begin(), hosts.end());
        hostList.sort();

        m_table->setRowCount(row + 1);
        auto* name = new QTableWidgetItem(s.name);
        name->setFont(uiFont(10, QFont::DemiBold));
        m_table->setItem(row, 0, name);
        m_table->setCellWidget(row, 1, TagBadge::cell(TagBadge::forSkillType(type)));
        auto* hostsItem = new QTableWidgetItem(hostList.join(", "));
        hostsItem->setToolTip(hostList.join(", "));
        m_table->setItem(row, 2, hostsItem);
        auto* cnt = new QTableWidgetItem(QString::number(s.locations.size()));
        cnt->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, cnt);
        m_rowToSkill.append(i);
        ++row;
    }
    m_table->setRowCount(row);
}

void SkillsPage::showDetail(int tableRow)
{
    while (m_detailBody->count() > 1) {
        QLayoutItem* it = m_detailBody->takeAt(1);
        delete it->widget();
        delete it;
    }
    if (tableRow < 0 || tableRow >= m_rowToSkill.size()) {
        m_detail->hide();
        return;
    }
    const NamedItem& s = m_inv.skills.at(m_rowToSkill.at(tableRow));
    m_detailTitle->setText(s.name + "  —  " + skillType(s));

    auto line = [&](const QString& k, const QString& v, bool mono) {
        auto* w = new QLabel(QString("%1  %2").arg(k, -14).arg(v));
        w->setFont(mono ? monoFont(8) : uiFont(9));
        w->setStyleSheet(QString("color:%1;").arg(Color::TextSecondary));
        w->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_detailBody->addWidget(w);
    };

    for (const auto& l : s.locations) {
        if (l.link.isLink) {
            line("canonical", l.link.target, true);
            if (l.git.detected)
                line("git", l.git.remote + " @ " + l.git.commit.left(10), true);
            break;
        }
    }
    for (const auto& l : s.locations)
        line(l.host, l.path + (l.link.isLink ? "  → " + l.link.linkType : QString()), true);

    m_detail->show();
}

} // namespace dm
