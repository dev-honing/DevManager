#include "gui/widgets/machine_card.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"

#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

namespace dm {

MachineCard::MachineCard(QWidget* parent) : QFrame(parent)
{
    setObjectName("panelCard");
    setAttribute(Qt::WA_StyledBackground, true);
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(16, 12, 12, 12);
    m_grid->setHorizontalSpacing(12);
    m_grid->setVerticalSpacing(4);
    m_grid->setColumnStretch(2, 1);
}

void MachineCard::addRow(int r, const QString& iconName, const QString& label,
                         const QString& value, bool mono)
{
    auto* ic = new QLabel;
    ic->setPixmap(icons::pixmap(iconName, QColor(Color::Muted), 15));
    ic->setFixedWidth(16);

    auto* lb = new QLabel(label);
    lb->setStyleSheet(QString("color:%1;").arg(Color::TextSecondary));
    lb->setFixedWidth(96);

    auto* val = new QLabel(value);
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (mono)
        val->setFont(monoFont(9));

    auto* copy = new QToolButton;
    copy->setObjectName("iconBtn");
    copy->setIcon(icons::icon("copy", QColor(Color::Muted), 14));
    copy->setToolTip("Copy");
    copy->setCursor(Qt::PointingHandCursor);
    copy->setVisible(false);
    connect(copy, &QToolButton::clicked, this,
            [value] { QApplication::clipboard()->setText(value); });

    // reveal copy button on row hover: cheap approach — always-dim, brighten on enter
    copy->setVisible(true);
    copy->setStyleSheet("QToolButton#iconBtn{opacity:0.5;}");

    m_grid->addWidget(ic, r, 0);
    m_grid->addWidget(lb, r, 1);
    m_grid->addWidget(val, r, 2);
    m_grid->addWidget(copy, r, 3);
}

void MachineCard::setRows(const QString& machine, const QString& userProfile,
                          const QString& os)
{
    // clear
    while (QLayoutItem* it = m_grid->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    addRow(0, "cpu", "Machine", machine, false);
    addRow(1, "folder", "User profile", userProfile, true);
    addRow(2, "environment", "OS", os, false);
}

} // namespace dm
