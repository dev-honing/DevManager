#pragma once
#include <QMainWindow>

#include "core/service/service_probe.h"
#include "gui/app_controller.h"

class QStackedWidget;

namespace dm {

class TopBar;
class Sidebar;
class RightPanel;
class SummaryCard;
class EnvironmentPage;
class SkillsPage;
class PluginsPage;
class PackagesPage;
class EnvVarsPage;
class SnapshotPage;
class RestorePage;
class SettingsPage;
class ProjectsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void selectPage(const QString& id);   // for --shot verification

private slots:
    void onScanStarted();
    void onScanFinished(const dm::EnvironmentInventory& inv);
    void onServicesProbed(const QList<dm::ServiceState>& services);
    void onNavSelected(const QString& id);
    void onSidebarAction(const QString& id);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void buildUi();

    AppController m_controller;

    TopBar* m_topBar = nullptr;
    Sidebar* m_sidebar = nullptr;
    RightPanel* m_rightPanel = nullptr;
    QStackedWidget* m_stack = nullptr;
    QString m_devRoot;

    SummaryCard* m_cardTools = nullptr;
    SummaryCard* m_cardSkills = nullptr;
    SummaryCard* m_cardLinked = nullptr;
    SummaryCard* m_cardPackages = nullptr;

    EnvironmentPage* m_envPage = nullptr;
    SkillsPage* m_skillsPage = nullptr;
    PluginsPage* m_pluginsPage = nullptr;
    PackagesPage* m_packagesPage = nullptr;
    EnvVarsPage* m_envVarsPage = nullptr;
    SnapshotPage* m_snapshotPage = nullptr;
    RestorePage* m_restorePage = nullptr;
    SettingsPage* m_settingsPage = nullptr;
    ProjectsPage* m_projectsPage = nullptr;

    QString m_backupsDir;
    QString m_configSource;
    QList<dm::ServiceState> m_lastServices;
    void refreshMigrationPages();
};

} // namespace dm
