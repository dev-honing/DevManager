#include "gui/pages/environment_page.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"
#include "gui/widgets/machine_card.h"
#include "gui/widgets/segmented_control.h"
#include "gui/widgets/status_badge.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace dm {

namespace {
struct Row {
    QString name;
    QString version;
    QString path;
};

QString prettyCategory(const QString& id)
{
    if (id.isEmpty())
        return "Other";
    static const QHash<QString, QString> special{
        {"ai", "AI Tools"}, {"vcs", "VCS"}, {"js", "JavaScript"}, {"ml", "ML"}};
    if (special.contains(id.toLower()))
        return special.value(id.toLower());
    QString s = id;
    s[0] = s[0].toUpper();
    return s;
}

// Rows for a category: every configured tool tagged with it, plus the
// synthetic entries that have no command of their own.
QVector<Row> rowsFor(const QString& cat, const EnvironmentInventory& inv)
{
    QVector<Row> rows;
    for (auto it = inv.toolCategories.constBegin();
         it != inv.toolCategories.constEnd(); ++it) {
        if (it.value() != cat)
            continue;
        rows.push_back({it.key(), inv.tools.value(it.key()),
                        inv.toolPaths.value(it.key())});
    }
    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.name < b.name; });

    if (cat == "build") {
        rows.push_back({"qt", inv.qt, {}});
        rows.push_back({"visual studio", inv.visualStudio, {}});
    }
    if (cat == "containers") {
        QString wsl;
        if (!inv.wsl.distributions.isEmpty())
            wsl = inv.wsl.distributions.first().state.toLower() + " ("
                  + QString::number(inv.wsl.distributions.size()) + " distro)";
        else if (inv.wsl.installed)
            wsl = "installed";
        rows.push_back({"wsl", wsl, {}});
    }
    return rows;
}
} // namespace

EnvironmentPage::EnvironmentPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    // header
    auto* head = new QHBoxLayout;
    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(1);
    auto* title = new QLabel("Environment");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("Machine, tools, versions and system information.");
    sub->setObjectName("pageSubtitle");
    titleBox->addWidget(title);
    titleBox->addWidget(sub);

    auto* copyAll = new QPushButton("  Copy all");
    copyAll->setObjectName("secondaryBtn");
    copyAll->setIcon(icons::icon("copy", QColor(Color::TextSecondary), 14));
    copyAll->setCursor(Qt::PointingHandCursor);
    connect(copyAll, &QPushButton::clicked, this, [this] {
        QStringList out;
        out << "machine\t" + m_inv.machine << "os\t" + m_inv.osCaption;
        for (auto it = m_inv.tools.constBegin(); it != m_inv.tools.constEnd(); ++it)
            out << it.key() + "\t" + it.value();
        QApplication::clipboard()->setText(out.join("\n"));
    });

    head->addLayout(titleBox);
    head->addStretch(1);
    head->addWidget(copyAll);
    lay->addLayout(head);

    // machine card
    m_machine = new MachineCard;
    lay->addWidget(m_machine);

    // segmented nav
    m_segments = new SegmentedControl;   // segments are built per-scan from config
    connect(m_segments, &SegmentedControl::changed, this, &EnvironmentPage::renderCategory);
    lay->addWidget(m_segments);

    // tools table
    m_table = new QTableWidget;
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Name", "Version", "Status", "Path", ""});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setDefaultSectionSize(Metric::RowHeight + 4);
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(2, QHeaderView::Fixed);
    hh->setSectionResizeMode(3, QHeaderView::Stretch);
    hh->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 104);
    m_table->setColumnWidth(4, 76);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideMiddle);
    lay->addWidget(m_table, 1);
}

void EnvironmentPage::setInventory(const EnvironmentInventory& inv)
{
    m_inv = inv;
    m_machine->setRows(inv.machine, inv.userProfile,
                       inv.osCaption + "  (" + inv.osVersion + ")");

    // Build the category segments from whatever the config actually defines.
    QStringList cats;
    for (auto it = inv.toolCategories.constBegin();
         it != inv.toolCategories.constEnd(); ++it)
        if (!cats.contains(it.value()))
            cats << it.value();
    for (const QString& forced : {QStringLiteral("build"), QStringLiteral("containers")})
        if (!cats.contains(forced))
            cats << forced;                 // qt/vs and wsl live here
    cats.sort();

    const QString prev = m_segments->current();
    m_segments->clear();
    for (const QString& c : cats)
        m_segments->addSegment(c, prettyCategory(c));
    if (cats.contains(prev))
        m_segments->setCurrent(prev);
    else if (!cats.isEmpty())
        renderCategory(m_segments->current());
}

void EnvironmentPage::renderCategory(const QString& id)
{
    const QVector<Row> rows = rowsFor(id, m_inv);
    m_table->clearContents();
    m_table->setRowCount(rows.size());

    for (int i = 0; i < rows.size(); ++i) {
        const Row& r = rows.at(i);
        const bool installed = !r.version.trimmed().isEmpty();

        auto* name = new QTableWidgetItem(r.name);
        name->setFont(uiFont(10, QFont::DemiBold));
        m_table->setItem(i, 0, name);

        auto* ver = new QTableWidgetItem(installed ? r.version : QStringLiteral("—"));
        ver->setFont(monoFont(9));
        ver->setForeground(QColor(installed ? Color::TextPrimary : Color::Muted));
        m_table->setItem(i, 1, ver);

        auto* badge = new StatusBadge;
        badge->set(installed ? StatusBadge::Ok : StatusBadge::Neutral,
                   installed ? "Installed" : "Not found");
        m_table->setCellWidget(i, 2, badge);

        auto* pathItem = new QTableWidgetItem(r.path.isEmpty() ? QString() : r.path);
        pathItem->setFont(monoFont(9));
        pathItem->setForeground(QColor(Color::TextSecondary));
        pathItem->setToolTip(r.path);
        pathItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_table->setItem(i, 3, pathItem);

        if (!r.path.isEmpty()) {
            auto* actions = new QWidget;
            auto* al = new QHBoxLayout(actions);
            al->setContentsMargins(0, 0, 6, 0);
            al->setSpacing(2);
            al->addStretch(1);

            auto* copy = new QToolButton;
            copy->setObjectName("iconBtn");
            copy->setIcon(icons::icon("copy", QColor(Color::Muted), 14));
            copy->setToolTip("Copy path");
            copy->setCursor(Qt::PointingHandCursor);
            const QString path = r.path;
            connect(copy, &QToolButton::clicked, this,
                    [path] { QApplication::clipboard()->setText(path); });

            auto* open = new QToolButton;
            open->setObjectName("iconBtn");
            open->setIcon(icons::icon("folder", QColor(Color::Muted), 14));
            open->setToolTip("Open containing folder");
            open->setCursor(Qt::PointingHandCursor);
            connect(open, &QToolButton::clicked, this, [path] {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            });

            al->addWidget(copy);
            al->addWidget(open);
            m_table->setCellWidget(i, 4, actions);
        }
    }
}

} // namespace dm
