#pragma once
#include <QWidget>

#include "model.h"

class QTableWidget;
class QLineEdit;
class QLabel;

namespace dm {

class SegmentedControl;

class PackagesPage : public QWidget {
    Q_OBJECT
public:
    explicit PackagesPage(QWidget* parent = nullptr);
    void setInventory(const EnvironmentInventory& inv);
    void setSearchFilter(const QString& text);

private:
    void rebuild();

    EnvironmentInventory m_inv;
    SegmentedControl* m_filter = nullptr;
    QLineEdit* m_search = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_count = nullptr;
};

} // namespace dm
