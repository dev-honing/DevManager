#pragma once
#include <QStringList>
#include <Qt>

class QTableWidget;

namespace dm {

// Shared setup for a read-only, non-editable, non-selectable list table --
// the pattern every page that just displays rows (no click-to-select detail
// view) was repeating by hand. Skills page keeps its own setup: it allows row
// selection to drive a detail panel, which this factory deliberately doesn't.
QTableWidget* makeListTable(const QStringList& headers, int rowHeight = 0,
                            Qt::TextElideMode elide = Qt::ElideMiddle);

} // namespace dm
