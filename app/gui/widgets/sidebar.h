#pragma once
#include <QWidget>
#include <QHash>
class QLabel;
class QVBoxLayout;

namespace dm {

class SidebarItem : public QWidget {
    Q_OBJECT
public:
    SidebarItem(QString id, const QString& iconName, const QString& text,
                bool footer, QWidget* parent = nullptr);
    QString id() const { return m_id; }
    void setSelected(bool on);
    void setCount(int n);

signals:
    void activated(const QString& id);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QString m_id;
    QString m_iconName;
    QLabel* m_icon = nullptr;
    QLabel* m_text = nullptr;
    QLabel* m_badge = nullptr;
    bool m_selected = false;
};

class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(QWidget* parent = nullptr);

    SidebarItem* addItem(const QString& id, const QString& iconName, const QString& text);
    void addSection(const QString& label);
    SidebarItem* addFooterItem(const QString& id, const QString& iconName, const QString& text);
    void setCount(const QString& id, int n);
    void setCurrent(const QString& id);
    QString current() const { return m_current; }

signals:
    void selected(const QString& id);

private:
    void onActivated(const QString& id);

    QVBoxLayout* m_lay = nullptr;
    QVBoxLayout* m_footLay = nullptr;
    QHash<QString, SidebarItem*> m_items;
    QString m_current;
};

} // namespace dm
