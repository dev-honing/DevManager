#pragma once
#include <QWidget>
class QButtonGroup;
class QHBoxLayout;

namespace dm {

// A small pill of mutually-exclusive segments (Tools / Build / ...).
class SegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedControl(QWidget* parent = nullptr);
    void addSegment(const QString& id, const QString& text, bool enabled = true);
    void clear();
    void setCurrent(const QString& id);
    QString current() const { return m_current; }

signals:
    void changed(const QString& id);

private:
    QHBoxLayout* m_lay = nullptr;
    QButtonGroup* m_group = nullptr;
    QString m_current;
    int m_nextId = 0;
};

} // namespace dm
