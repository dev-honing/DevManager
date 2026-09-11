#include "gui/pages/projects_page.h"

#include "gui/theme.h"

#include <QtConcurrent>

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/project/project_env.h"

namespace dm {

ProjectsPage::ProjectsPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Projects");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    lay->addWidget(title);
    lay->addWidget(new QLabel("Generate a per-project dev container (P11) and bring it up/down."));

    auto* pathRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit;
    m_pathEdit->setPlaceholderText("Project folder...");
    connect(m_pathEdit, &QLineEdit::textChanged, this, &ProjectsPage::refreshButtons);
    m_browseBtn = new QPushButton("Browse...");
    m_browseBtn->setObjectName("secondaryBtn");
    connect(m_browseBtn, &QPushButton::clicked, this, &ProjectsPage::browse);
    pathRow->addWidget(new QLabel("Folder:"));
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(m_browseBtn);
    lay->addLayout(pathRow);

    auto* metaRow = new QHBoxLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("project name");
    m_typeCombo = new QComboBox;
    for (const QString& t : ProjectEnv::availableTypes())
        m_typeCombo->addItem(t);
    metaRow->addWidget(new QLabel("Name:"));
    metaRow->addWidget(m_nameEdit, 1);
    metaRow->addWidget(new QLabel("Type:"));
    metaRow->addWidget(m_typeCombo);
    lay->addLayout(metaRow);

    auto* btnRow = new QHBoxLayout;
    m_generateBtn = new QPushButton("  Generate Files");
    m_generateBtn->setObjectName("dryCheckBtn");
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_generateBtn, &QPushButton::clicked, this, &ProjectsPage::generate);
    m_upBtn = new QPushButton("Up");
    connect(m_upBtn, &QPushButton::clicked, this, &ProjectsPage::confirmUp);
    m_downBtn = new QPushButton("Down");
    m_downBtn->setObjectName("secondaryBtn");
    connect(m_downBtn, &QPushButton::clicked, this, &ProjectsPage::confirmDown);
    btnRow->addWidget(m_generateBtn);
    btnRow->addWidget(m_upBtn);
    btnRow->addWidget(m_downBtn);
    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    m_status = new QLabel;
    m_status->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    lay->addWidget(m_status);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setFont(monoFont(9));
    m_log->setPlaceholderText("Output from generate / docker compose up / down.");
    lay->addWidget(m_log, 1);

    connect(&m_upWatcher, &QFutureWatcher<ComposeResult>::finished, this,
            [this] { onComposeResult(m_upWatcher.result(), "up"); });
    connect(&m_downWatcher, &QFutureWatcher<ComposeResult>::finished, this,
            [this] { onComposeResult(m_downWatcher.result(), "down"); });

    refreshButtons();
}

void ProjectsPage::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Project folder");
    if (dir.isEmpty())
        return;
    m_pathEdit->setText(dir);
    if (m_nameEdit->text().trimmed().isEmpty())
        m_nameEdit->setText(QFileInfo(dir).fileName());
}

void ProjectsPage::refreshButtons()
{
    const bool haveDir = !m_pathEdit->text().trimmed().isEmpty();
    m_generateBtn->setEnabled(haveDir);
    const bool haveCompose = haveDir && ProjectControl::hasCompose(m_pathEdit->text().trimmed());
    m_upBtn->setEnabled(haveCompose && !m_upWatcher.isRunning());
    m_downBtn->setEnabled(haveCompose && !m_downWatcher.isRunning());
    if (haveDir && !haveCompose)
        m_status->setText("No .devmanager/docker-compose.yml yet -- click Generate Files.");
}

void ProjectsPage::log(const QString& line)
{
    m_log->appendPlainText(line);
}

void ProjectsPage::generate()
{
    const QString dir = m_pathEdit->text().trimmed();
    const QString name = m_nameEdit->text().trimmed().isEmpty()
                             ? QFileInfo(dir).fileName()
                             : m_nameEdit->text().trimmed();
    if (dir.isEmpty() || !QFileInfo(dir).isDir()) {
        m_status->setText("Pick an existing project folder first.");
        return;
    }
    const ProjectSpec spec = ProjectEnv::resolve(name, m_typeCombo->currentText());
    if (!spec.valid) {
        m_status->setText("error: " + spec.error);
        return;
    }
    if (!spec.docker) {
        m_status->setText(QString("'%1' is a host profile (%2) -- no container. "
                                  "Check readiness from Settings > Health instead.")
                              .arg(spec.type, spec.hostProfile));
        return;
    }
    const ProjectWriteResult w = ProjectControl::writeFiles(spec, dir);
    if (!w.ok) {
        m_status->setText("error: " + w.error);
        log("error: " + w.error);
        return;
    }
    m_status->setText(QString("Wrote .devmanager/docker-compose.yml + .devcontainer/ "
                              "(image %1, volumes %2)")
                          .arg(spec.image, spec.volumes.join(", ")));
    log("generated compose for '" + name + "' (" + spec.image + ") in " + dir);
    refreshButtons();
}

void ProjectsPage::confirmUp()
{
    const QString dir = m_pathEdit->text().trimmed();
    if (QMessageBox::question(this, "Start container",
                              "Run  docker compose up -d  for this project now?",
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;
    m_upBtn->setEnabled(false);
    m_status->setText("starting...");
    m_upWatcher.setFuture(QtConcurrent::run([dir] { return ProjectControl::up(dir); }));
}

void ProjectsPage::confirmDown()
{
    const QString dir = m_pathEdit->text().trimmed();
    if (QMessageBox::question(this, "Stop container",
                              "Run  docker compose down  for this project now?",
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes)
        return;
    m_downBtn->setEnabled(false);
    m_status->setText("stopping...");
    m_downWatcher.setFuture(QtConcurrent::run([dir] { return ProjectControl::down(dir); }));
}

void ProjectsPage::onComposeResult(const ComposeResult& r, const QString& verb)
{
    m_status->setText(QString("compose %1: %2").arg(verb, r.ok ? "ok" : "failed"));
    log("--- compose " + verb + " (" + (r.ok ? "ok" : "failed") + ") ---");
    log(r.output.trimmed());
    refreshButtons();
}

} // namespace dm
