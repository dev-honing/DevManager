#include "gui/main_window.h"
#include "gui/theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
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
    t->setRootIsDecorated(true);
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
    t->verticalHeader()->setDefaultSectionSize(30);
    return t;
}

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

// ---------------------------------------------------------------- window

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("DevManager");
    resize(1120, 720);
    buildUi();

    connect(&m_controller, &AppController::scanStarted, this, &MainWindow::onScanStarted);
    connect(&m_controller, &AppController::scanFinished, this, &MainWindow::onScanFinished);
    m_controller.scanEnvironment();
}

QWidget* MainWindow::buildHeader()
{
    auto* bar = new QFrame;
    bar->setObjectName("header");
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(20, 14, 16, 14);
    lay->setSpacing(12);

    auto* title = new QLabel("DevManager");
    title->setObjectName("appTitle");

    m_headerStatus = new QLabel("idle");
    m_headerStatus->setObjectName("headerStatus");

    m_scanButton = new QPushButton("Rescan");
    m_scanButton->setObjectName("primaryBtn");
    m_scanButton->setCursor(Qt::PointingHandCursor);
    connect(m_scanButton, &QPushButton::clicked, &m_controller, &AppController::scanEnvironment);

    lay->addWidget(title);
    lay->addStretch(1);
    lay->addWidget(m_headerStatus);
    lay->addWidget(m_scanButton);
    return bar;
}

static QFrame* statCard(const QString& caption, QLabel** valueOut)
{
    auto* card = new QFrame;
    card->setObjectName("statCard");
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(2);
    auto* value = new QLabel("-");
    value->setObjectName("statValue");
    auto* cap = new QLabel(caption);
    cap->setObjectName("statCaption");
    lay->addWidget(value);
    lay->addWidget(cap);
    *valueOut = value;
    return card;
}

QWidget* MainWindow::buildStatRow()
{
    auto* row = new QWidget;
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(20, 16, 20, 8);
    lay->setSpacing(12);
    lay->addWidget(statCard("Tools", &m_statTools));
    lay->addWidget(statCard("Skills", &m_statSkills));
    lay->addWidget(statCard("Linked locations", &m_statLinked));
    lay->addWidget(statCard("Packages", &m_statPackages));
    lay->addStretch(1);
    return row;
}

QWidget* MainWindow::wrapPage(QWidget* content)
{
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(20, 12, 20, 16);
    lay->addWidget(content);
    return page;
}

void MainWindow::buildUi()
{
    m_envTree = makeTree({"Item", "Value"});
    m_skillTree = makeTree({"Skill / Host", "Classification", "Link", "Target", "Git"});
    m_pluginTree = makeTree({"Plugin / Host", "Classification", "Link", "Path"});

    // packages page: filter + table
    auto* pkgPage = new QWidget;
    auto* pkgLay = new QVBoxLayout(pkgPage);
    pkgLay->setContentsMargins(0, 0, 0, 0);
    pkgLay->setSpacing(10);
    m_packageFilter = new QLineEdit;
    m_packageFilter->setPlaceholderText("Filter packages by name...");
    m_packageFilter->setClearButtonEnabled(true);
    connect(m_packageFilter, &QLineEdit::textChanged, this, &MainWindow::filterPackages);
    m_packageTable = makeTable({"Manager", "Name", "Version"});
    pkgLay->addWidget(m_packageFilter);
    pkgLay->addWidget(m_packageTable);

    m_envVarTable = makeTable({"Name", "Value (masked)"});

    m_stack = new QStackedWidget;
    m_stack->addWidget(wrapPage(m_envTree));
    m_stack->addWidget(wrapPage(m_skillTree));
    m_stack->addWidget(wrapPage(m_pluginTree));
    m_stack->addWidget(wrapPage(pkgPage));
    m_stack->addWidget(wrapPage(m_envVarTable));

    m_nav = new QListWidget;
    m_nav->setObjectName("nav");
    m_nav->setFixedWidth(190);
    m_nav->setFrameShape(QFrame::NoFrame);
    for (const QString& label : {"Environment", "Skills", "Plugins", "Packages", "Env Vars"})
        m_nav->addItem(label);
    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto* body = new QWidget;
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    bodyLay->addWidget(m_nav);
    bodyLay->addWidget(m_stack, 1);

    auto* central = new QWidget;
    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(buildHeader());
    outer->addWidget(buildStatRow());
    outer->addWidget(body, 1);
    setCentralWidget(central);

    statusBar()->showMessage("ready");
}

void MainWindow::onScanStarted()
{
    m_scanButton->setEnabled(false);
    m_headerStatus->setText("scanning...");
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

    m_statTools->setText(QString::number(inv.tools.size()));
    m_statSkills->setText(QString::number(inv.skills.size()));
    m_statLinked->setText(QString::number(linked));
    m_statPackages->setText(QString::number(inv.globalPackages.size()));

    m_scanButton->setEnabled(true);
    m_headerStatus->setText("scanned " + inv.capturedAt.left(19).replace('T', ' '));
    statusBar()->showMessage(inv.machine + "  ·  " + inv.osCaption);
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

    m_envTree->expandAll();
}

void MainWindow::populateSkills(const EnvironmentInventory& inv)
{
    m_skillTree->clear();
    QColor accentColor(QString::fromLatin1(accentHex()));
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
        auto* none = new QTreeWidgetItem(m_pluginTree, {"(none discovered)"});
        none->setForeground(0, QColor("#9aa0a6"));
        return;
    }
    for (const auto& p : inv.plugins) {
        auto* top = new QTreeWidgetItem(m_pluginTree, {p.name});
        top->setExpanded(true);
        for (const auto& l : p.locations)
            new QTreeWidgetItem(top, {l.host, l.classification,
                                      l.link.isLink ? l.link.linkType : QStringLiteral("-"),
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
