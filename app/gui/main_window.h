#pragma once
#include <QMainWindow>

#include "gui/app_controller.h"

class QTreeWidget;
class QTableWidget;
class QLineEdit;
class QLabel;
class QAction;

namespace dm {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onScanStarted();
    void onScanFinished(const dm::EnvironmentInventory& inv);
    void filterPackages(const QString& text);

private:
    void buildUi();
    void populateEnvironment(const EnvironmentInventory& inv);
    void populateSkills(const EnvironmentInventory& inv);
    void populatePlugins(const EnvironmentInventory& inv);
    void populatePackages(const EnvironmentInventory& inv);
    void populateEnvVars(const EnvironmentInventory& inv);

    AppController m_controller;

    QAction* m_scanAction = nullptr;
    QLabel* m_summary = nullptr;

    QTreeWidget* m_envTree = nullptr;
    QTreeWidget* m_skillTree = nullptr;
    QTreeWidget* m_pluginTree = nullptr;
    QTableWidget* m_packageTable = nullptr;
    QLineEdit* m_packageFilter = nullptr;
    QTableWidget* m_envVarTable = nullptr;
};

} // namespace dm
