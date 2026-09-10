#pragma once
#include <QMainWindow>

#include "gui/app_controller.h"

class QTreeWidget;
class QTableWidget;
class QLineEdit;
class QLabel;
class QListWidget;
class QStackedWidget;
class QPushButton;

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
    QWidget* buildHeader();
    QWidget* buildStatRow();
    QWidget* wrapPage(QWidget* content);
    void populateEnvironment(const EnvironmentInventory& inv);
    void populateSkills(const EnvironmentInventory& inv);
    void populatePlugins(const EnvironmentInventory& inv);
    void populatePackages(const EnvironmentInventory& inv);
    void populateEnvVars(const EnvironmentInventory& inv);

    AppController m_controller;

    QPushButton* m_scanButton = nullptr;
    QLabel* m_headerStatus = nullptr;
    QListWidget* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;

    QLabel* m_statTools = nullptr;
    QLabel* m_statSkills = nullptr;
    QLabel* m_statLinked = nullptr;
    QLabel* m_statPackages = nullptr;

    QTreeWidget* m_envTree = nullptr;
    QTreeWidget* m_skillTree = nullptr;
    QTreeWidget* m_pluginTree = nullptr;
    QTableWidget* m_packageTable = nullptr;
    QLineEdit* m_packageFilter = nullptr;
    QTableWidget* m_envVarTable = nullptr;
};

} // namespace dm
