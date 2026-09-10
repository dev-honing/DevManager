#include "gui/widgets/summary_card.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"

#include <QGridLayout>
#include <QLabel>

namespace dm {

SummaryCard::SummaryCard(const QString& iconName, const QString& label,
                         const QString& accentHex, QWidget* parent)
    : QFrame(parent), m_iconName(iconName), m_accent(accentHex)
{
    setObjectName("summaryCard");
    setMinimumHeight(84);
    setMaximumHeight(92);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* g = new QGridLayout(this);
    g->setContentsMargins(Metric::CardPadding, 12, Metric::CardPadding, 12);
    g->setHorizontalSpacing(12);
    g->setVerticalSpacing(0);

    m_icon = new QLabel;
    m_icon->setPixmap(icons::pixmap(iconName, QColor(accentHex), 20));
    m_icon->setFixedSize(34, 34);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setStyleSheet(QString("background:rgba(255,255,255,0.04);border-radius:9px;"));

    m_value = new QLabel("-");
    m_value->setObjectName("cardValue");
    m_value->setFont(uiFont(23, QFont::Bold, /*display=*/true));

    m_label = new QLabel(label);
    m_label->setObjectName("cardLabel");

    m_hint = new QLabel;
    m_hint->setObjectName("cardHint");
    m_hint->hide();

    g->addWidget(m_icon, 0, 0, 3, 1, Qt::AlignVCenter);
    g->addWidget(m_value, 0, 1);
    g->addWidget(m_label, 1, 1);
    g->addWidget(m_hint, 2, 1);
    g->setColumnStretch(1, 1);
}

void SummaryCard::setValue(const QString& value) { m_value->setText(value); }

void SummaryCard::setHint(const QString& hint)
{
    m_hint->setText(hint);
    m_hint->setVisible(!hint.isEmpty());
}

} // namespace dm
