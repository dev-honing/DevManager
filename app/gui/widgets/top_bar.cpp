#include "gui/widgets/top_bar.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace dm {

TopBar::TopBar(QWidget* parent) : QFrame(parent)
{
    setObjectName("topBar");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(58);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 10, 16, 10);
    lay->setSpacing(12);

    // --- left: logo + title
    auto* logo = new QLabel("D");
    logo->setObjectName("appLogo");
    logo->setFixedSize(28, 28);
    logo->setAlignment(Qt::AlignCenter);
    logo->setFont(uiFont(13, QFont::Black, /*display=*/true));

    auto* titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    auto* title = new QLabel("DevManager");
    title->setObjectName("appTitle");
    title->setFont(uiFont(14, QFont::Bold, /*display=*/true));
    m_subtitle = new QLabel("Dev environment dashboard");
    m_subtitle->setObjectName("appSubtitle");
    titleBox->addWidget(title);
    titleBox->addWidget(m_subtitle);

    // --- center: search
    m_search = new QLineEdit;
    m_search->setObjectName("topSearch");
    m_search->setPlaceholderText("Search tools, skills, packages...");
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(420);
    m_search->addAction(icons::icon("search", QColor(Color::Muted), 15),
                        QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, &TopBar::searchChanged);

    // --- right: last scanned + refresh + rescan
    m_lastScanned = new QLabel("never scanned");
    m_lastScanned->setObjectName("lastScanned");

    auto* refresh = new QToolButton;
    refresh->setObjectName("iconBtn");
    refresh->setIcon(icons::icon("refresh", QColor(Color::TextSecondary), 16));
    refresh->setToolTip("Rescan environment");
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QToolButton::clicked, this, &TopBar::rescanRequested);

    m_rescan = new QPushButton("Rescan");
    m_rescan->setObjectName("primaryBtn");
    m_rescan->setCursor(Qt::PointingHandCursor);
    m_rescan->setIcon(icons::icon("refresh", QColor("#ffffff"), 15));
    m_rescan->setToolTip("Re-run the read-only environment scan");
    connect(m_rescan, &QPushButton::clicked, this, &TopBar::rescanRequested);

    lay->addWidget(logo);
    lay->addLayout(titleBox);
    lay->addSpacing(8);
    lay->addStretch(1);
    lay->addWidget(m_search, 2);
    lay->addStretch(1);
    lay->addWidget(m_lastScanned);
    lay->addWidget(refresh);
    lay->addWidget(m_rescan);
}

void TopBar::setLastScanned(const QString& iso)
{
    QString shown = iso;
    shown.replace('T', ' ');
    if (shown.size() > 19)
        shown = shown.left(19);
    m_lastScanned->setText("Last scanned  " + shown);
}

void TopBar::setBusy(bool busy)
{
    m_rescan->setEnabled(!busy);
    m_rescan->setText(busy ? "Scanning..." : "Rescan");
}

void TopBar::setCompact(bool compact)
{
    m_subtitle->setVisible(!compact);
    m_search->setVisible(!compact);
}

QString TopBar::searchText() const { return m_search->text(); }

} // namespace dm
