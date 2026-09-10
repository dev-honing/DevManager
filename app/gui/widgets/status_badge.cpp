#include "gui/widgets/status_badge.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"

#include <QHBoxLayout>
#include <QLabel>

namespace dm {

StatusBadge::StatusBadge(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(2, 0, 8, 0);
    lay->setSpacing(6);
    m_dot = new QLabel;
    m_dot->setFixedSize(10, 10);
    m_text = new QLabel;
    m_text->setFont(uiFont(9));
    m_text->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    lay->addWidget(m_dot, 0, Qt::AlignVCenter);
    lay->addWidget(m_text, 0, Qt::AlignVCenter);
    set(Neutral, "-");
}

void StatusBadge::set(Level level, const QString& text)
{
    const char* c = Color::Muted;
    switch (level) {
    case Ok: c = Color::Success; break;
    case Warn: c = Color::Warning; break;
    case Error: c = Color::Danger; break;
    case Neutral: c = Color::Muted; break;
    }
    m_dot->setPixmap(icons::pixmap("dot", QColor(c), 10));
    m_text->setText(text);
    m_text->setStyleSheet(QString("color:%1;").arg(level == Neutral ? Color::TextSecondary : c));
}

} // namespace dm
