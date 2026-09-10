#include "gui/main_window.h"

#include "gui/pages/environment_page.h"
#include "gui/pages/placeholder_page.h"
#include "gui/theme.h"
#include "gui/widgets/icons.h"
#include "gui/widgets/sidebar.h"
#include "gui/widgets/summary_card.h"
#include "gui/widgets/top_bar.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace dm {

// ---------------------------------------------------------------- helpers

static QTreeWidget* makeTree(const QStringList& headers)
{
    auto* t = new QTreeWidget;
    t->setColumnCount(headers.size());
    t->setHeaderLabels(headers);
    t->setUniformRowHeights(true);
    t->setAlternatingRowColors(true);
    t->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    t->header()->setStretchLastSection(true);
    return t;
}

static QTableWidget* makeTable(const QStringList& headers)
{
    auto* t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setShowGrid(false);
    t->setAlternatingRowColors(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setDefaultSectionSize(Metric::RowHeight);
    return t;
}

static QWidget* pad(QWidget* content)
{
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->addWidget(content);
    return page;
}

// ---------------------------------------------------------------- window

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("DevManager");
    resize(1240, 760);
    buildUi();

    connect(&m_controller, &AppController::scanStarted, this, &MainWindow::onScanStarted);
    connect(&m_controller, &AppController::scanFinished, this, &MainWindow::onScanFinished);
    m_controller.scanEnvironment();
}

void MainWindow::buildUi()
{
    // --- top bar
    m_topBar = new TopBar;
    connect(m_topBar, &TopBar::rescanRequested, &m_controller, &AppController::scanEnvironment);
    connect(m_topBar, &TopBar::searchChanged, this, [this](const QString& t) {
        if (m_sidebar->current() == "packages")
            filterPackages(t);
    });

    // --- sidebar
    m_sidebar = new Sidebar;
    m_sidebar->addItem("environment", "environment", "Environment");
    m_sidebar->addItem("skills", "skills", "Skills");
    m_sidebar->addItem("plugins", "plugins", "Plugins");
    m_sidebar->addItem("packages", "packages", "Packages");
    m_sidebar->addItem("envvars", "env", "Env Vars");
    m_sidebar->addSection("Migration");
    m_sidebar->addItem("snapshots", "snapshots", "Snapshots");
    m_sidebar->addItem("restore", "restore", "Restore");
    m_sidebar->addItem("settings", "settings", "Settings");
    m_sidebar->addFooterItem("docs", "docs", "Documentation");
    m_sidebar->addFooterItem("about", "about", "About");
    connect(m_sidebar, &Sidebar::selected, this, &MainWindow::onNavSelected);

    // --- summary cards
    m_cardTools = new SummaryCard("cpu", "Tools", Color::Primary);
    m_cardSkills = new SummaryCard("skills", "Skills", Color::Success);
    m_cardLinked = new SummaryCard("plugins", "Linked locations", Color::Purple);
    m_cardPackages = new SummaryCard("packages", "Packages", Color::Amber);

    auto* cardRow = new QWidget;
    auto* cardLay = new QHBoxLayout(cardRow);
    cardLay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, 0);
    cardLay->setSpacing(Metric::Gap);
    for (auto* c : {m_cardTools, m_cardSkills, m_cardLinked, m_cardPackages})
        cardLay->addWidget(c);

    // --- pages
    m_stack = new QStackedWidget;
    m_envPage = new EnvironmentPage;
    m_stack->addWidget(m_envPage);                         // 0 environment
    m_stack->addWidget(buildSkillsPage());                 // 1 skills
    m_stack->addWidget(buildPluginsPage());                // 2 plugins
    m_stack->addWidget(buildPackagesPage());               // 3 packages
    m_stack->addWidget(buildEnvVarsPage());                // 4 envvars
    m_stack->addWidget(new PlaceholderPage(                // 5 snapshots
        "snapshots", "Snapshots",
        "Snapshot planning arrives in a later phase."));
    m_stack->addWidget(new PlaceholderPage(                // 6 restore
        "restore", "Restore",
        "Restore (dry-run first) arrives in a later phase."));
    m_stack->addWidget(new PlaceholderPage(                // 7 settings
        "settings", "Settings", "Nothing to configure yet."));

    // --- assemble
    auto* rightSide = new QWidget;
    auto* rightLay = new QVBoxLayout(rightSide);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);
    rightLay->addWidget(cardRow);
    rightLay->addWidget(m_stack, 1);

    auto* body = new QWidget;
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    bodyLay->addWidget(m_sidebar);
    bodyLay->addWidget(rightSide, 1);

    auto* central = new QWidget;
    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_topBar);
    outer->addWidget(body, 1);
    setCentralWidget(central);

    statusBar()->showMessage("ready");
    m_sidebar->setCurrent("environment");
}

QWidget* MainWindow::buildSkillsPage()
{
    m_skillTree = makeTree({"Skill / Host", "Type", "Link", "Target", "Git"});
    return pad(m_skillTree);
}

QWidget* MainWindow::buildPluginsPage()
{
    m_pluginTree = makeTree({"Plugin / Host", "Type", "Link", "Path"});
    return pad(m_pluginTree);
}

QWidget* MainWindow::buildPackagesPage()
{
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(10);
    m_packageFilter = new QLineEdit;
    m_packageFilter->setPlaceholderText("Filter packages by name...");
    m_packageFilter->setClearButtonEnabled(true);
    connect(m_packageFilter, &QLineEdit::textChanged, this, &MainWindow::filterPackages);
    m_packageTable = makeTable({"Manager", "Name", "Version"});
    lay->addWidget(m_packageFilter);
    lay->addWidget(m_packageTable);
    return page;
}

QWidget* MainWindow::buildEnvVarsPage()
{
    m_envVarTable = makeTable({"Name", "Value (masked)"});
    return pad(m_envVarTable);
}

void MainWindow::selectPage(const QString& id) { m_sidebar->setCurrent(id); }

void MainWindow::onNavSelected(const QString& id)
{
    static const QHash<QString, int> map{
        {"environment", 0}, {"skills", 1}, {"plugins", 2}, {"packages", 3},
        {"envvars", 4}, {"snapshots", 5}, {"restore", 6}, {"settings", 7}};
    if (map.contains(id))
        m_stack->setCurrentIndex(map.value(id));
}

void MainWindow::onScanStarted()
{
    m_topBar->setBusy(true);
    statusBar()->showMessage("scanning environment (read-only)...");
}

void MainWindow::onScanFinished(const EnvironmentInventory& inv)
{
    m_envPage->setInventory(inv);
    populateSkills(inv);
    populatePlugins(inv);
    populatePackages(inv);
    populateEnvVars(inv);

    int linked = 0;
    for (const auto& s : inv.skills)
        for (const auto& l : s.locations)
            if (l.link.isLink) ++linked;

    int toolsOk = 0;
    for (auto it = inv.tools.constBegin(); it != inv.tools.constEnd(); ++it)
        if (!it.value().trimmed().isEmpty()) ++toolsOk;

    m_cardTools->setValue(QString::number(inv.tools.size()));
    m_cardTools->setHint(QString("%1 installed").arg(toolsOk));
    m_cardSkills->setValue(QString::number(inv.skills.size()));
    m_cardLinked->setValue(QString::number(linked));
    m_cardPackages->setValue(QString::number(inv.globalPackages.size()));

    m_sidebar->setCount("skills", inv.skills.size());
    m_sidebar->setCount("plugins", inv.plugins.size());
    m_sidebar->setCount("packages", inv.globalPackages.size());

    m_topBar->setBusy(false);
    m_topBar->setLastScanned(inv.capturedAt);
    statusBar()->showMessage(inv.machine + "  •  " + inv.osCaption);
}

void MainWindow::populateSkills(const EnvironmentInventory& inv)
{
    m_skillTree->clear();
    const QColor accentColor(QString::fromLatin1(accentHex()));
    const QBrush accent(accentColor);

    for (const auto& s : inv.skills) {
        auto* top = new QTreeWidgetItem(m_skillTree, {s.name});
        QFont f = top->font(0);
        f.setBold(true);
        top->setFont(0, f);
        top->setExpanded(true);

        for (const auto& l : s.locations) {
            const QString git =
                l.git.detected
                    ? QString("%1 @ %2").arg(l.git.remote, l.git.commit.left(10))
                    : QString();
            auto* row = new QTreeWidgetItem(
                top, {l.host, l.classification,
                      l.link.isLink ? l.link.linkType : QStringLiteral("-"),
                      l.link.isLink ? l.link.target : QString(), git});
            row->setFont(3, monoFont());
            row->setFont(4, monoFont());
            if (l.link.isLink)
                for (int c = 0; c < 5; ++c)
                    row->setForeground(c, accent);
        }
    }
    for (int c = 1; c < 5; ++c)
        m_skillTree->resizeColumnToContents(c);
}

void MainWindow::populatePlugins(const EnvironmentInventory& inv)
{
    m_pluginTree->clear();
    if (inv.plugins.isEmpty()) {
        auto* none = new QTreeWidgetItem(m_pluginTree, {"No plugins discovered."});
        none->setForeground(0, QColor(Color::Muted));
        auto* hint = new QTreeWidgetItem(m_pluginTree, {"Plugin roots were scanned successfully."});
        hint->setForeground(0, QColor(Color::Muted));
        return;
    }
    for (const auto& p : inv.plugins) {
        auto* top = new QTreeWidgetItem(m_pluginTree, {p.name});
        top->setExpanded(true);
        for (const auto& l : p.locations) {
            auto* row = new QTreeWidgetItem(
                top, {l.host, l.classification,
                      l.link.isLink ? l.link.linkType : QStringLiteral("-"), l.path});
            row->setFont(3, monoFont());
        }
    }
}

void MainWindow::populatePackages(const EnvironmentInventory& inv)
{
    m_packageTable->setRowCount(inv.globalPackages.size());
    for (int i = 0; i < inv.globalPackages.size(); ++i) {
        const auto& p = inv.globalPackages.at(i);
        m_packageTable->setItem(i, 0, new QTableWidgetItem(p.manager));
        m_packageTable->setItem(i, 1, new QTableWidgetItem(p.name));
        auto* ver = new QTableWidgetItem(p.version);
        ver->setFont(monoFont());
        m_packageTable->setItem(i, 2, ver);
    }
    m_packageTable->resizeColumnToContents(0);
    m_packageTable->resizeColumnToContents(1);
    filterPackages(m_packageFilter->text());
}

void MainWindow::populateEnvVars(const EnvironmentInventory& inv)
{
    m_envVarTable->setRowCount(inv.env.size());
    int r = 0;
    for (auto it = inv.env.constBegin(); it != inv.env.constEnd(); ++it, ++r) {
        m_envVarTable->setItem(r, 0, new QTableWidgetItem(it.key()));
        auto* val = new QTableWidgetItem(it.value());
        val->setFont(monoFont());
        m_envVarTable->setItem(r, 1, val);
    }
    m_envVarTable->resizeColumnToContents(0);
}

void MainWindow::filterPackages(const QString& text)
{
    for (int i = 0; i < m_packageTable->rowCount(); ++i) {
        const auto* name = m_packageTable->item(i, 1);
        const bool hit = text.isEmpty()
            || (name && name->text().contains(text, Qt::CaseInsensitive));
        m_packageTable->setRowHidden(i, !hit);
    }
}

} // namespace dm
