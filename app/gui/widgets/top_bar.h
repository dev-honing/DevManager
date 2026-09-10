#pragma once
#include <QFrame>
class QLineEdit;
class QLabel;
class QPushButton;

namespace dm {

class TopBar : public QFrame {
    Q_OBJECT
public:
    explicit TopBar(QWidget* parent = nullptr);

    void setLastScanned(const QString& iso);
    void setBusy(bool busy);
    QString searchText() const;

signals:
    void rescanRequested();
    void searchChanged(const QString& text);

private:
    QLineEdit* m_search = nullptr;
    QLabel* m_lastScanned = nullptr;
    QPushButton* m_rescan = nullptr;
};

} // namespace dm
