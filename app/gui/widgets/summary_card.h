#pragma once
#include <QFrame>
class QLabel;

namespace dm {

// icon + big number + label (+ optional hint). Accent tints the icon only.
class SummaryCard : public QFrame {
    Q_OBJECT
public:
    SummaryCard(const QString& iconName, const QString& label,
                const QString& accentHex, QWidget* parent = nullptr);

    void setValue(const QString& value);
    void setHint(const QString& hint);

private:
    QLabel* m_icon = nullptr;
    QLabel* m_value = nullptr;
    QLabel* m_label = nullptr;
    QLabel* m_hint = nullptr;
    QString m_iconName;
    QString m_accent;
};

} // namespace dm
