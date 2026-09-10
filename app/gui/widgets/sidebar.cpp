#include "gui/widgets/sidebar.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

namespace dm {

// ---------------------------------------------------------------- SidebarItem

SidebarItem::SidebarItem(QString id, const QString& iconName, const QString& text,
                         bool footer, QWidget* parent)
    : QWidget(parent), m_id(std::move(id)), m_iconName(iconName)
{
    setObjectName(footer ? "navFootItem" : "navItem");
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("selected", false);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(12, footer ? 6 : 8, 10, footer ? 6 : 8);
    lay->setSpacing(10);

    m_icon = new QLabel;
    const int px = footer ? 14 : 16;
    m_icon->setPixmap(icons::pixmap(iconName, QColor(Color::TextSecondary), px));
    m_icon->setFixedWidth(px);

    m_text = new QLabel(text);
    m_text->setFont(uiFont(footer ? 9 : 10));

    m_badge = new QLabel;
    m_badge->setObjectName("navBadge");
    m_badge->hide();

    lay->addWidget(m_icon);
    lay->addWidget(m_text);
    lay->addStretch(1);
    lay->addWidget(m_badge);
}

void SidebarItem::setSelected(bool on)
{
    if (m_selected == on)
        return;
    m_selected = on;
    setProperty("selected", on);
    m_icon->setPixmap(icons::pixmap(
        m_iconName, QColor(on ? Color::TextPrimary : Color::TextSecondary), 16));
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void SidebarItem::setCount(int n)
{
    if (n < 0) {
        m_badge->hide();
        return;
    }
    m_badge->setText(QString::number(n));
    m_badge->show();
}

void SidebarItem::paintEvent(QPaintEvent*)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SidebarItem::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        emit activated(m_id);
}

void SidebarItem::enterEvent(QEnterEvent*)
{
    if (!m_selected)
        m_icon->setPixmap(icons::pixmap(m_iconName, QColor(Color::TextPrimary), 16));
}

void SidebarItem::leaveEvent(QEvent*)
{
    if (!m_selected)
        m_icon->setPixmap(icons::pixmap(m_iconName, QColor(Color::TextSecondary), 16));
}

// ---------------------------------------------------------------- Sidebar

Sidebar::Sidebar(QWidget* parent) : QWidget(parent)
{
    setObjectName("sidebar");
    setFixedWidth(Metric::SidebarWidth);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 10, 8, 10);
    outer->setSpacing(2);

    m_lay = new QVBoxLayout;
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(2);

    m_footLay = new QVBoxLayout;
    m_footLay->setContentsMargins(0, 0, 0, 0);
    m_footLay->setSpacing(2);

    outer->addLayout(m_lay);
    outer->addStretch(1);
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("color:%1;").arg(Color::Border));
    outer->addWidget(line);
    outer->addLayout(m_footLay);
}

SidebarItem* Sidebar::addItem(const QString& id, const QString& iconName,
                              const QString& text)
{
    auto* it = new SidebarItem(id, iconName, text, /*footer=*/false);
    connect(it, &SidebarItem::activated, this, &Sidebar::onActivated);
    m_lay->addWidget(it);
    m_items.insert(id, it);
    return it;
}

void Sidebar::addSection(const QString& label)
{
    auto* l = new QLabel(label.toUpper());
    l->setObjectName("navSep");
    m_lay->addWidget(l);
}

SidebarItem* Sidebar::addFooterItem(const QString& id, const QString& iconName,
                                    const QString& text)
{
    auto* it = new SidebarItem(id, iconName, text, /*footer=*/true);
    connect(it, &SidebarItem::activated, this, &Sidebar::onActivated);
    m_footLay->addWidget(it);
    m_items.insert(id, it);
    return it;
}

void Sidebar::setCount(const QString& id, int n)
{
    if (auto* it = m_items.value(id))
        it->setCount(n);
}

void Sidebar::setCurrent(const QString& id)
{
    if (id == m_current || !m_items.contains(id))
        return;
    if (auto* prev = m_items.value(m_current))
        prev->setSelected(false);
    m_current = id;
    m_items.value(id)->setSelected(true);
    emit selected(id);
}

void Sidebar::onActivated(const QString& id) { setCurrent(id); }

} // namespace dm
