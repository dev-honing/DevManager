#pragma once
#include <QWidget>

#include "model.h"

class QTableWidget;
class QFrame;
class QLabel;
class QVBoxLayout;

namespace dm {

class SegmentedControl;

class SkillsPage : public QWidget {
    Q_OBJECT
public:
    explicit SkillsPage(QWidget* parent = nullptr);
    void setInventory(const EnvironmentInventory& inv);
    void setSearchFilter(const QString& text);

private:
    void rebuild();
    void showDetail(int tableRow);
    static QString skillType(const NamedItem& s);

    EnvironmentInventory m_inv;
    SegmentedControl* m_filter = nullptr;
    QTableWidget* m_table = nullptr;
    QFrame* m_detail = nullptr;
    QVBoxLayout* m_detailBody = nullptr;
    QLabel* m_detailTitle = nullptr;
    QString m_search;
    QVector<int> m_rowToSkill;   // table row -> index into m_inv.skills
};

} // namespace dm
