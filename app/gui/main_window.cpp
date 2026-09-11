#include "gui/main_window.h"

#include "core/snapshot/snapshot_index.h"
#include "gui/pages/env_vars_page.h"
#include "gui/pages/environment_page.h"
#include "gui/pages/packages_page.h"
#include "gui/pages/plugins_page.h"
#include "gui/pages/projects_page.h"
#include "gui/pages/restore_page.h"
#include "gui/pages/settings_page.h"
#include "gui/pages/skills_page.h"
#include "gui/pages/snapshot_page.h"
#include "gui/theme.h"
#include "gui/widgets/right_panel.h"
#include "gui/widgets/sidebar.h"
#include "gui/widgets/summary_card.h"
#include "gui/widgets/top_bar.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace dm {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("DevManager");
    resize(1240, 760);
    buildUi();

    connect(&m_controller, &AppController::scanStarted, this, &MainWindow::onScanStarted);
    connect(&m_controller, &AppController::scanFinished, this, &MainWindow::onScanFinished);
    connect(&m_controller, &AppController::servicesProbed, this, &MainWindow::onServicesProbed);
    m_controller.scanEnvironment();
}

void MainWindow::buildUi()
{
    m_topBar = new TopBar;
    connect(m_topBar, &TopBar::rescanRequested, &m_controller, &AppController::scanEnvironment);
    connect(m_topBar, &TopBar::searchChanged, this, [this](const QString& t) {
        const QString p = m_sidebar->current();
        if (p == "skills") m_skillsPage->setSearchFilter(t);
        else if (p == "packages") m_packagesPage->setSearchFilter(t);
        else if (p == "envvars") m_envVarsPage->setSearchFilter(t);
    });

    m_sidebar = new Sidebar;
    m_sidebar->addItem("environment", "environment", "Environment");
    m_sidebar->addItem("skills", "skills", "Skills");
    m_sidebar->addItem("plugins", "plugins", "Plugins");
    m_sidebar->addItem("packages", "packages", "Packages");
    m_sidebar->addItem("envvars", "env", "Env Vars");
    m_sidebar->addSection("Migration");
    m_sidebar->addItem("snapshots", "snapshots", "Snapshots");
    m_sidebar->addItem("restore", "restore", "Restore");
    m_sidebar->addSection("Containers");
    m_sidebar->addItem("projects", "container", "Projects");
    m_sidebar->addSection("General");
    m_sidebar->addItem("settings", "settings", "Settings");
    m_sidebar->addFooterItem("docs", "docs", "Documentation");
    m_sidebar->addFooterItem("about", "about", "About");
    connect(m_sidebar, &Sidebar::selected, this, &MainWindow::onNavSelected);
    connect(m_sidebar, &Sidebar::actionSelected, this, &MainWindow::onSidebarAction);

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

    m_stack = new QStackedWidget;
    m_envPage = new EnvironmentPage;
    m_skillsPage = new SkillsPage;
    m_pluginsPage = new PluginsPage;
    m_packagesPage = new PackagesPage;
    m_envVarsPage = new EnvVarsPage;
    m_stack->addWidget(m_envPage);        // 0
    m_stack->addWidget(m_skillsPage);     // 1
    m_stack->addWidget(m_pluginsPage);    // 2
    m_stack->addWidget(m_packagesPage);   // 3
    m_stack->addWidget(m_envVarsPage);    // 4
    m_snapshotPage = new SnapshotPage;
    m_restorePage = new RestorePage;
    connect(m_snapshotPage, &SnapshotPage::snapshotCreated, this, [this] {
        m_rightPanel->setSnapshots(SnapshotIndex::list(m_backupsDir));
        m_restorePage->setContext(m_backupsDir, m_lastServices);
    });
    connect(m_restorePage, &RestorePage::restoreApplied, this, [this] {
        m_controller.scanEnvironment();
    });
    m_stack->addWidget(m_snapshotPage);   // 5
    m_stack->addWidget(m_restorePage);    // 6
    m_projectsPage = new ProjectsPage;
    m_stack->addWidget(m_projectsPage);   // 7
    m_settingsPage = new SettingsPage;
    connect(m_settingsPage, &SettingsPage::snapshotsPruned, this, [this] {
        m_rightPanel->setSnapshots(SnapshotIndex::list(m_backupsDir));
    });
    m_stack->addWidget(m_settingsPage);   // 8

    m_rightPanel = new RightPanel;
    connect(m_rightPanel, &RightPanel::scanRequested,
            &m_controller, &AppController::scanEnvironment);
    connect(m_rightPanel, &RightPanel::navigateTo, this,
            [this](const QString& id) { m_sidebar->setCurrent(id); });
    connect(m_rightPanel, &RightPanel::openDevFolderRequested, this, [this] {
        if (!m_devRoot.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_devRoot));
    });
    connect(m_rightPanel, &RightPanel::serviceControlRequested, this,
            [this](const QString& id, LifecycleOp op) {
                const QString verb = lifecycleOpName(op);
                if (QMessageBox::question(
                        this, "Service control",
                        QString("%1 %2 now?\n\nThis changes the running service.")
                            .arg(verb.left(1).toUpper() + verb.mid(1), id),
                        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                    == QMessageBox::Yes)
                    m_controller.controlService(id, op);
            });
    connect(&m_controller, &AppController::serviceControlled, this,
            [this](const LifecycleResult& r) {
                statusBar()->showMessage(
                    QString("%1 %2: %3")
                        .arg(r.serviceId, lifecycleOpName(r.op),
                             r.ok ? QStringLiteral("ok") : r.error),
                    6000);
            });

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
    bodyLay->addWidget(m_rightPanel);

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

void MainWindow::selectPage(const QString& id) { m_sidebar->setCurrent(id); }

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    if (m_rightPanel)
        m_rightPanel->setVisible(width() >= 1160);
    if (m_topBar)
        m_topBar->setCompact(width() < 1000);
}

void MainWindow::onSidebarAction(const QString& id)
{
    if (id == "docs") {
        QDesktopServices::openUrl(
            QUrl("https://github.com/dev-honing/DevManager"));
    } else if (id == "about") {
        QMessageBox box(this);
        box.setWindowTitle("About DevManager");
        box.setTextFormat(Qt::RichText);
        box.setText(
            "<b>DevManager</b><br>AI dev environment dashboard<br><br>"
            "Qt " + QString(qVersion())
            + "<br>config: <code>"
            + (m_configSource.isEmpty() ? "built-in defaults" : m_configSource)
            + "</code><br><br>"
            "<a href='https://github.com/dev-honing/DevManager'>"
            "github.com/dev-honing/DevManager</a>");
        box.setTextInteractionFlags(Qt::TextBrowserInteraction);
        box.exec();
    }
}

void MainWindow::onServicesProbed(const QList<ServiceState>& services)
{
    m_lastServices = services;
    m_rightPanel->setServices(services);
    refreshMigrationPages();
}

void MainWindow::refreshMigrationPages()
{
    m_snapshotPage->setServices(m_lastServices);
    m_snapshotPage->setBackupsDir(m_backupsDir);
    m_restorePage->setContext(m_backupsDir, m_lastServices);
    m_settingsPage->setBackupsDir(m_backupsDir);
    m_settingsPage->setServiceContext(m_lastServices, m_lastEnv);
}

void MainWindow::onNavSelected(const QString& id)
{
    static const QHash<QString, int> map{
        {"environment", 0}, {"skills", 1}, {"plugins", 2}, {"packages", 3},
        {"envvars", 4}, {"snapshots", 5}, {"restore", 6}, {"projects", 7},
        {"settings", 8}};
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
    m_skillsPage->setInventory(inv);
    m_pluginsPage->setInventory(inv);
    m_packagesPage->setInventory(inv);
    m_envVarsPage->setInventory(inv);
    m_lastEnv = inv.env;

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

    m_backupsDir = SnapshotIndex::findBackupsDir(inv.root);
    m_devRoot = m_backupsDir.isEmpty() ? inv.root
                                       : QFileInfo(m_backupsDir).absolutePath();
    m_rightPanel->setSnapshots(SnapshotIndex::list(m_backupsDir));
    refreshMigrationPages();

    m_configSource = inv.configSource;
    m_topBar->setBusy(false);
    m_topBar->setLastScanned(inv.capturedAt);

    const QString cfg = inv.configSource.isEmpty()
                            ? QStringLiteral("built-in defaults")
                            : QFileInfo(inv.configSource).fileName();
    statusBar()->showMessage(inv.machine + "  •  " + inv.osCaption
                             + "   ·   config: " + cfg);
    statusBar()->setToolTip(inv.configSource);
}

} // namespace dm
