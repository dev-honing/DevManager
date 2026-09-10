#include "gui/widgets/tag_badge.h"

#include "gui/theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QWidget>

namespace dm {

TagBadge::TagBadge(QWidget* parent) : QLabel(parent)
{
    setFont(uiFont(8, QFont::DemiBold));
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize TagBadge::sizeHint() const
{
    const QFontMetrics fm(font());
    return {fm.horizontalAdvance(text()) + 18, fm.height() + 8};
}

void TagBadge::setTag(const QString& text, const QString& colorHex)
{
    setText(text.toUpper());
    QColor c(colorHex);
    setStyleSheet(QString("background:rgba(%1,%2,%3,0.16); color:%4;"
                          "border-radius:5px; padding:2px 7px;")
                      .arg(c.red())
                      .arg(c.green())
                      .arg(c.blue())
                      .arg(colorHex));
    adjustSize();
}

QWidget* TagBadge::cell(TagBadge* badge)
{
    auto* w = new QWidget;
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(8, 0, 8, 0);
    l->setSpacing(0);
    l->addWidget(badge, 0, Qt::AlignVCenter | Qt::AlignLeft);
    l->addStretch(1);
    return w;
}

TagBadge* TagBadge::forSkillType(const QString& classification, QWidget* parent)
{
    auto* b = new TagBadge(parent);
    if (classification == "linked")
        b->setTag("Linked", Color::Purple);
    else if (classification == "system")
        b->setTag("System", Color::TextSecondary);
    else
        b->setTag("User", Color::Success);
    return b;
}

} // namespace dm
