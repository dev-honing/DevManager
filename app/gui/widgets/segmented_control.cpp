#include "gui/widgets/segmented_control.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

namespace dm {

SegmentedControl::SegmentedControl(QWidget* parent) : QWidget(parent)
{
    auto* track = new QWidget(this);
    track->setObjectName("segTrack");
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(track);
    outer->addStretch(1);

    m_lay = new QHBoxLayout(track);
    m_lay->setContentsMargins(3, 3, 3, 3);
    m_lay->setSpacing(2);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
}

void SegmentedControl::addSegment(const QString& id, const QString& text, bool enabled)
{
    auto* b = new QToolButton;
    b->setObjectName("segBtn");
    b->setText(text);
    b->setCheckable(true);
    b->setEnabled(enabled);
    b->setCursor(Qt::PointingHandCursor);
    b->setProperty("segId", id);
    m_group->addButton(b, m_nextId++);
    m_lay->addWidget(b);

    connect(b, &QToolButton::clicked, this, [this, id] {
        m_current = id;
        emit changed(id);
    });

    if (m_current.isEmpty() && enabled) {
        b->setChecked(true);
        m_current = id;
    }
}

void SegmentedControl::setCurrent(const QString& id)
{
    for (auto* btn : m_group->buttons()) {
        if (btn->property("segId").toString() == id) {
            btn->setChecked(true);
            m_current = id;
            emit changed(id);
            return;
        }
    }
}

} // namespace dm
