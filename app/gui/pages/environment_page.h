#pragma once
#include <QWidget>

#include "model.h"

class QTableWidget;
class QLabel;

namespace dm {

class MachineCard;
class SegmentedControl;

class EnvironmentPage : public QWidget {
    Q_OBJECT
public:
    explicit EnvironmentPage(QWidget* parent = nullptr);
    void setInventory(const EnvironmentInventory& inv);

private:
    void renderCategory(const QString& id);

    EnvironmentInventory m_inv;
    MachineCard* m_machine = nullptr;
    SegmentedControl* m_segments = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace dm
