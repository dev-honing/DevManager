#include "gui/pages/placeholder_page.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"

#include <QLabel>
#include <QVBoxLayout>

namespace dm {

PlaceholderPage::PlaceholderPage(const QString& iconName, const QString& title,
                                const QString& subtitle, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setAlignment(Qt::AlignCenter);
    lay->setSpacing(8);

    auto* ic = new QLabel;
    ic->setPixmap(icons::pixmap(iconName, QColor(Color::Border), 44));
    ic->setAlignment(Qt::AlignCenter);

    auto* t = new QLabel(title);
    t->setFont(uiFont(14, QFont::DemiBold, true));
    t->setAlignment(Qt::AlignCenter);

    auto* s = new QLabel(subtitle);
    s->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    s->setAlignment(Qt::AlignCenter);

    lay->addWidget(ic);
    lay->addWidget(t);
    lay->addWidget(s);
}

} // namespace dm
