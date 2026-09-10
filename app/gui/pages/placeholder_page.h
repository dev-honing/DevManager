#pragma once
#include <QWidget>

namespace dm {

// Empty-state page for sections whose backend arrives in a later phase.
class PlaceholderPage : public QWidget {
    Q_OBJECT
public:
    PlaceholderPage(const QString& iconName, const QString& title,
                    const QString& subtitle, QWidget* parent = nullptr);
};

} // namespace dm
