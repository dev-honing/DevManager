#include "gui/widgets/right_panel.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"
#include "gui/widgets/status_badge.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace dm {

static StatusBadge::Level toBadge(ServiceState::Level l)
{
    switch (l) {
    case ServiceState::Running: return StatusBadge::Ok;
    case ServiceState::Warning: return StatusBadge::Warn;
    case ServiceState::Error: return StatusBadge::Error;
    default: return StatusBadge::Neutral;
    }
}
static QString levelText(ServiceState::Level l)
{
    switch (l) {
    case ServiceState::Running: return "Running";
    case ServiceState::Stopped: return "Stopped";
    case ServiceState::Warning: return "Warning";
    case ServiceState::Error: return "Error";
    default: return "Unknown";
    }
}

RightPanel::RightPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("rightPanel");
    setFixedWidth(Metric::RightPanelWidth);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString("#rightPanel{background:%1;border-left:1px solid %2;}")
                      .arg(Color::Bg, Color::Border));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 16, 14, 16);
    outer->setSpacing(Metric::Gap);

    // --- System Status
    auto* statusBody = new QWidget;
    m_statusRows = new QVBoxLayout(statusBody);
    m_statusRows->setContentsMargins(0, 0, 0, 0);
    m_statusRows->setSpacing(8);
    outer->addWidget(section("System Status", statusBody));

    // --- Quick Actions
    auto* actionsBody = new QWidget;
    auto* al = new QVBoxLayout(actionsBody);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(6);

    auto mkBtn = [&](const QString& text, const QString& icon, bool primary,
                     const QString& tip) {
        auto* b = new QPushButton("  " + text);
        b->setObjectName(primary ? "primaryBtn" : "secondaryBtn");
        b->setIcon(icons::icon(icon, QColor(primary ? "#ffffff" : Color::TextSecondary), 14));
        b->setCursor(Qt::PointingHandCursor);
        b->setLayoutDirection(Qt::LeftToRight);
        b->setToolTip(tip);
        al->addWidget(b);
        return b;
    };
    connect(mkBtn("Scan Environment", "refresh", true,
                  "Re-run the read-only environment scan"),
            &QPushButton::clicked, this, &RightPanel::scanRequested);
    connect(mkBtn("Create Snapshot", "snapshots", false,
                  "Open the Snapshots page (dry check)"),
            &QPushButton::clicked, this, [this] { emit navigateTo("snapshots"); });
    connect(mkBtn("Restore from Snapshot", "restore", false,
                  "Open the Restore page (dry check)"),
            &QPushButton::clicked, this, [this] { emit navigateTo("restore"); });
    connect(mkBtn("Manage Skills", "skills", false, "Open the Skills page"),
            &QPushButton::clicked, this, [this] { emit navigateTo("skills"); });
    connect(mkBtn("Open Dev Folder", "folder", false,
                  "Open the project folder in Explorer"),
            &QPushButton::clicked, this, &RightPanel::openDevFolderRequested);
    outer->addWidget(section("Quick Actions", actionsBody));

    // --- Recent Snapshots
    auto* snapBody = new QWidget;
    m_snapshotRows = new QVBoxLayout(snapBody);
    m_snapshotRows->setContentsMargins(0, 0, 0, 0);
    m_snapshotRows->setSpacing(6);
    outer->addWidget(section("Recent Snapshots", snapBody));

    outer->addStretch(1);
}

QWidget* RightPanel::section(const QString& title, QWidget* body)
{
    auto* card = new QFrame;
    card->setObjectName("panelCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(10);

    auto* head = new QLabel(title.toUpper());
    head->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;"
                                "letter-spacing:0.8px;")
                            .arg(Color::Muted));
    lay->addWidget(head);
    lay->addWidget(body);
    return card;
}

void RightPanel::setServices(const QList<ServiceState>& services)
{
    while (QLayoutItem* it = m_statusRows->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    for (const ServiceState& s : services) {
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);

        auto* name = new QLabel(s.name);
        name->setFont(uiFont(10));

        auto* badge = new StatusBadge;
        badge->set(toBadge(s.level), levelText(s.level));
        badge->setToolTip(s.detail);

        rl->addWidget(name);
        rl->addStretch(1);
        rl->addWidget(badge);
        m_statusRows->addWidget(row);
    }
}

void RightPanel::setSnapshots(const QList<SnapshotSummary>& snapshots)
{
    while (QLayoutItem* it = m_snapshotRows->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    if (snapshots.isEmpty()) {
        auto* empty = new QLabel("No snapshots yet.");
        empty->setStyleSheet(QString("color:%1;").arg(Color::Muted));
        m_snapshotRows->addWidget(empty);
        return;
    }

    const int shown = qMin(4, snapshots.size());
    for (int i = 0; i < shown; ++i) {
        const auto& s = snapshots.at(i);
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);

        auto* dot = new QLabel;
        dot->setPixmap(icons::pixmap(
            "dot", QColor(s.consistent ? Color::Success : Color::Muted), 9));
        dot->setFixedWidth(10);

        auto* box = new QVBoxLayout;
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(0);
        auto* when = new QLabel(s.stamp);
        when->setFont(monoFont(8));
        auto* cnt = new QLabel(QString("%1 components").arg(s.componentCount));
        cnt->setStyleSheet(QString("color:%1;font-size:10px;").arg(Color::Muted));
        box->addWidget(when);
        box->addWidget(cnt);

        rl->addWidget(dot);
        rl->addLayout(box);
        rl->addStretch(1);
        m_snapshotRows->addWidget(row);
    }

    auto* viewAll = new QPushButton("View all");
    viewAll->setObjectName("secondaryBtn");
    viewAll->setCursor(Qt::PointingHandCursor);
    connect(viewAll, &QPushButton::clicked, this, [this] { emit navigateTo("snapshots"); });
    m_snapshotRows->addWidget(viewAll);
}

} // namespace dm
