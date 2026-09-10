#pragma once
#include <QFrame>
class QGridLayout;

namespace dm {

// Machine / User profile / OS rows, each with a hover "copy" button.
class MachineCard : public QFrame {
    Q_OBJECT
public:
    explicit MachineCard(QWidget* parent = nullptr);
    void setRows(const QString& machine, const QString& userProfile, const QString& os);

private:
    void addRow(int r, const QString& iconName, const QString& label,
                const QString& value, bool mono);
    QGridLayout* m_grid = nullptr;
};

} // namespace dm
