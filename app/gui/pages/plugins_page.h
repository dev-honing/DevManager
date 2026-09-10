#pragma once
#include <QWidget>

#include "model.h"

class QTableWidget;
class QStackedWidget;
class QLabel;

namespace dm {

class PluginsPage : public QWidget {
    Q_OBJECT
public:
    explicit PluginsPage(QWidget* parent = nullptr);
    void setInventory(const EnvironmentInventory& inv);

private:
    QStackedWidget* m_stack = nullptr;   // 0 = empty state, 1 = table
    QTableWidget* m_table = nullptr;
    QLabel* m_emptyHint = nullptr;
};

} // namespace dm
