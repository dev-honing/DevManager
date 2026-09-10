#include "gui/main_window.h"

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace dm {

static QTreeWidgetItem* section(QTreeWidget* t, const QString& name)
{
    auto* it = new QTreeWidgetItem(t, {name});
    QFont f = it->font(0);
    f.setBold(true);
    it->setFont(0, f);
    it->setFirstColumnSpanned(true);
    it->setExpanded(true);
    return it;
}

static void kv(QTreeWidgetItem* parent, const QString& k, const QString& v)
{
    new QTreeWidgetItem(parent, {k, v});
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("DevManager");
    resize(1000, 680);
    buildUi();

    connect(&m_controller, &AppController::scanStarted, this, &MainWindow::onScanStarted);
    connect(&m_controller, &AppController::scanFinished, this, &MainWindow::onScanFinished);

    // kick off an initial scan
    m_controller.scanEnvironment();
}

void MainWindow::buildUi()
{
    auto* tb = addToolBar("Main");
    tb->setMovable(false);
    m_scanAction = tb->addAction("Rescan");
    connect(m_scanAction, &QAction::triggered, &m_controller, &AppController::scanEnvironment);
    tb->addSeparator();
    m_summary = new QLabel("  idle");
    tb->addWidget(m_summary);

    auto* tabs = new QTabWidget;
    setCentralWidget(tabs);

    // Environment
    m_envTree = new QTreeWidget;
    m_envTree->setColumnCount(2);
    m_envTree->setHeaderLabels({"Item", "Value"});
    m_envTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tabs->addTab(m_envTree, "Environment");

    // Skills
    m_skillTree = new QTreeWidget;
    m_skillTree->setColumnCount(5);
    m_skillTree->setHeaderLabels({"Skill / Host", "Classification", "Link", "Target", "Git"});
    m_skillTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tabs->addTab(m_skillTree, "Skills");

    // Plugins
    m_pluginTree = new QTreeWidget;
    m_pluginTree->setColumnCount(4);
    m_pluginTree->setHeaderLabels({"Plugin / Host", "Classification", "Link", "Path"});
    m_pluginTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tabs->addTab(m_pluginTree, "Plugins");

    // Packages
    auto* pkgPage = new QWidget;
    auto* pkgLay = new QVBoxLayout(pkgPage);
    pkgLay->setContentsMargins(0, 0, 0, 0);
    m_packageFilter = new QLineEdit;
    m_packageFilter->setPlaceholderText("filter packages...");
    connect(m_packageFilter, &QLineEdit::textChanged, this, &MainWindow::filterPackages);
    m_packageTable = new QTableWidget;
    m_packageTable->setColumnCount(3);
    m_packageTable->setHorizontalHeaderLabels({"Manager", "Name", "Version"});
    m_packageTable->horizontalHeader()->setStretchLastSection(true);
    m_packageTable->verticalHeader()->setVisible(false);
    m_packageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pkgLay->addWidget(m_packageFilter);
    pkgLay->addWidget(m_packageTable);
    tabs->addTab(pkgPage, "Packages");

    // Env vars
    m_envVarTable = new QTableWidget;
    m_envVarTable->setColumnCount(2);
    m_envVarTable->setHorizontalHeaderLabels({"Name", "Value (masked)"});
    m_envVarTable->horizontalHeader()->setStretchLastSection(true);
    m_envVarTable->verticalHeader()->setVisible(false);
    m_envVarTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs->addTab(m_envVarTable, "Env Vars");

    statusBar()->showMessage("ready");
}

void MainWindow::onScanStarted()
{
    m_scanAction->setEnabled(false);
    m_summary->setText("  scanning...");
    statusBar()->showMessage("scanning environment (read-only)...");
}

void MainWindow::onScanFinished(const EnvironmentInventory& inv)
{
    populateEnvironment(inv);
    populateSkills(inv);
    populatePlugins(inv);
    populatePackages(inv);
    populateEnvVars(inv);

    int linked = 0;
    for (const auto& s : inv.skills)
        for (const auto& l : s.locations)
            if (l.link.isLink) ++linked;

    m_summary->setText(QString("  %1 tools  |  %2 skills (%3 linked locs)  |  %4 packages")
                           .arg(inv.tools.size())
                           .arg(inv.skills.size())
                           .arg(linked)
                           .arg(inv.globalPackages.size()));
    m_scanAction->setEnabled(true);
    statusBar()->showMessage("scan complete: " + inv.capturedAt);
}

void MainWindow::populateEnvironment(const EnvironmentInventory& inv)
{
    m_envTree->clear();

    auto* m = section(m_envTree, "Machine");
    kv(m, "Machine", inv.machine);
    kv(m, "User profile", inv.userProfile);
    kv(m, "OS", inv.osCaption + "  (" + inv.osVersion + ")");

    auto* tools = section(m_envTree, "Tools");
    for (auto it = inv.tools.constBegin(); it != inv.tools.constEnd(); ++it)
        kv(tools, it.key(), it.value().isEmpty() ? "(not found)" : it.value());

    auto* build = section(m_envTree, "Build");
    kv(build, "Qt", inv.qt);
    kv(build, "Visual Studio", inv.visualStudio);

    auto* wsl = section(m_envTree, "WSL");
    kv(wsl, "Installed", inv.wsl.installed ? "yes" : "no");
    for (const auto& d : inv.wsl.distributions)
        kv(wsl, d.name, QString("%1  (v%2)").arg(d.state).arg(d.version));
}

void MainWindow::populateSkills(const EnvironmentInventory& inv)
{
    m_skillTree->clear();
    const QBrush linkBrush(QColor(0x1a6, 0x5a, 0xba));

    for (const auto& s : inv.skills) {
        auto* top = new QTreeWidgetItem(m_skillTree, {s.name});
        QFont f = top->font(0);
        f.setBold(true);
        top->setFont(0, f);
        top->setExpanded(true);

        for (const auto& l : s.locations) {
            const QString git = l.git.detected
                ? QString("%1 @ %2").arg(l.git.remote,
                                         l.git.commit.left(10))
                : QString();
            auto* row = new QTreeWidgetItem(
                top, {l.host, l.classification,
                      l.link.isLink ? l.link.linkType : QString("-"),
                      l.link.isLink ? l.link.target : QString(), git});
            if (l.link.isLink) {
                for (int c = 0; c < 5; ++c)
                    row->setForeground(c, linkBrush);
            }
        }
    }
    for (int c = 1; c < 5; ++c)
        m_skillTree->resizeColumnToContents(c);
}

void MainWindow::populatePlugins(const EnvironmentInventory& inv)
{
    m_pluginTree->clear();
    if (inv.plugins.isEmpty()) {
        new QTreeWidgetItem(m_pluginTree, {"(none discovered)"});
        return;
    }
    for (const auto& p : inv.plugins) {
        auto* top = new QTreeWidgetItem(m_pluginTree, {p.name});
        top->setExpanded(true);
        for (const auto& l : p.locations)
            new QTreeWidgetItem(top, {l.host, l.classification,
                                      l.link.isLink ? l.link.linkType : QString("-"),
                                      l.path});
    }
}

void MainWindow::populatePackages(const EnvironmentInventory& inv)
{
    m_packageTable->setRowCount(inv.globalPackages.size());
    for (int i = 0; i < inv.globalPackages.size(); ++i) {
        const auto& p = inv.globalPackages.at(i);
        m_packageTable->setItem(i, 0, new QTableWidgetItem(p.manager));
        m_packageTable->setItem(i, 1, new QTableWidgetItem(p.name));
        m_packageTable->setItem(i, 2, new QTableWidgetItem(p.version));
    }
    m_packageTable->resizeColumnsToContents();
    filterPackages(m_packageFilter->text());
}

void MainWindow::populateEnvVars(const EnvironmentInventory& inv)
{
    m_envVarTable->setRowCount(inv.env.size());
    int r = 0;
    for (auto it = inv.env.constBegin(); it != inv.env.constEnd(); ++it, ++r) {
        m_envVarTable->setItem(r, 0, new QTableWidgetItem(it.key()));
        m_envVarTable->setItem(r, 1, new QTableWidgetItem(it.value()));
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
