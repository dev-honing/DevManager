#pragma once
#include <QWidget>
#include <QFutureWatcher>

#include "core/project/project_control.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace dm {

// Per-project dev container setup (P11.2/11.3): pick a project folder + type,
// generate .devmanager/docker-compose.yml + .devcontainer/devcontainer.json,
// then bring the container up or down via `docker compose`.
class ProjectsPage : public QWidget {
    Q_OBJECT
public:
    explicit ProjectsPage(QWidget* parent = nullptr);

private:
    void browse();
    void generate();
    void confirmUp();
    void confirmDown();
    void onComposeResult(const ComposeResult& r, const QString& verb);
    void refreshButtons();
    void log(const QString& line);

    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QPushButton* m_generateBtn = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_downBtn = nullptr;
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_log = nullptr;

    QFutureWatcher<ComposeResult> m_upWatcher;
    QFutureWatcher<ComposeResult> m_downWatcher;
};

} // namespace dm
