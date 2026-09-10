#pragma once
#include <QWidget>
class QLabel;

namespace dm {

// Small "● text" status pill. Reused for tool status and (later) service status.
class StatusBadge : public QWidget {
    Q_OBJECT
public:
    enum Level { Neutral, Ok, Warn, Error };

    explicit StatusBadge(QWidget* parent = nullptr);
    void set(Level level, const QString& text);

private:
    QLabel* m_dot = nullptr;
    QLabel* m_text = nullptr;
};

} // namespace dm
