#pragma once
#include <QWidget>

#include "model.h"

class QTableWidget;
class QLabel;

namespace dm {

class EnvVarsPage : public QWidget {
    Q_OBJECT
public:
    explicit EnvVarsPage(QWidget* parent = nullptr);
    void setInventory(const EnvironmentInventory& inv);
    void setSearchFilter(const QString& text);

private:
    void rebuild();
    static QString classify(const QString& name, const QString& value);

    EnvironmentInventory m_inv;
    QTableWidget* m_table = nullptr;
    QLabel* m_count = nullptr;
    QString m_search;
};

} // namespace dm
