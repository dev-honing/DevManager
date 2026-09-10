#pragma once
#include <QMainWindow>

#include "gui/app_controller.h"

class QTreeWidget;
class QTableWidget;
class QLineEdit;
class QLabel;
class QStackedWidget;

namespace dm {

class TopBar;
class Sidebar;
class SummaryCard;
class EnvironmentPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void selectPage(const QString& id);   // for --shot verification

private slots:
    void onScanStarted();
    void onScanFinished(const dm::EnvironmentInventory& inv);
    void onNavSelected(const QString& id);
    void filterPackages(const QString& text);

private:
    void buildUi();
    QWidget* buildSkillsPage();
    QWidget* buildPluginsPage();
    QWidget* buildPackagesPage();
    QWidget* buildEnvVarsPage();
    void populateSkills(const EnvironmentInventory& inv);
    void populatePlugins(const EnvironmentInventory& inv);
    void populatePackages(const EnvironmentInventory& inv);
    void populateEnvVars(const EnvironmentInventory& inv);

    AppController m_controller;

    TopBar* m_topBar = nullptr;
    Sidebar* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;

    SummaryCard* m_cardTools = nullptr;
    SummaryCard* m_cardSkills = nullptr;
    SummaryCard* m_cardLinked = nullptr;
    SummaryCard* m_cardPackages = nullptr;

    EnvironmentPage* m_envPage = nullptr;
    QTreeWidget* m_skillTree = nullptr;
    QTreeWidget* m_pluginTree = nullptr;
    QTableWidget* m_packageTable = nullptr;
    QLineEdit* m_packageFilter = nullptr;
    QTableWidget* m_envVarTable = nullptr;
};

} // namespace dm
