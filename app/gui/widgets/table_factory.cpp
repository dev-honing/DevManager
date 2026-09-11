#include "gui/widgets/table_factory.h"

#include "gui/theme.h"

#include <QHeaderView>
#include <QTableWidget>

namespace dm {

QTableWidget* makeListTable(const QStringList& headers, int rowHeight,
                            Qt::TextElideMode elide)
{
    auto* t = new QTableWidget;
    t->setTextElideMode(elide);
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->verticalHeader()->setDefaultSectionSize(rowHeight > 0 ? rowHeight : Metric::RowHeight);
    t->horizontalHeader()->setStretchLastSection(true);
    return t;
}

} // namespace dm
