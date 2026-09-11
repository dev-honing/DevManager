#include "project/project_control.h"

#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

using namespace dm;

class TstProjectControl : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void writeFilesCreatesComposeAndDevcontainer();
    void writeFilesRejectsHostProfileSpec();

private:
    QTemporaryDir* m_tmp = nullptr;
};

void TstProjectControl::init()
{
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
}

void TstProjectControl::cleanup()
{
    delete m_tmp;
    m_tmp = nullptr;
}

void TstProjectControl::writeFilesCreatesComposeAndDevcontainer()
{
    ProjectSpec s;
    s.valid = true;
    s.docker = true;
    s.name = "demo";
    s.type = "cpp";
    s.image = "ai-dev-cpp:0.1";
    s.workspace = "/workspace";
    s.containerHome = "/home/node";
    s.volumes = {"workspace", "claude"};

    QVERIFY(!ProjectControl::hasCompose(m_tmp->path()));

    const ProjectWriteResult w = ProjectControl::writeFiles(s, m_tmp->path());
    QVERIFY2(w.ok, w.error.toUtf8());
    QVERIFY(QFileInfo::exists(w.composePath));
    QVERIFY(QFileInfo::exists(m_tmp->path() + "/.devcontainer/devcontainer.json"));
    QVERIFY(ProjectControl::hasCompose(m_tmp->path()));

    QFile f(w.composePath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("ai-dev-cpp:0.1"));
}

void TstProjectControl::writeFilesRejectsHostProfileSpec()
{
    ProjectSpec s;
    s.valid = true;
    s.docker = false;
    s.hostProfile = "msvc-qt6";
    s.type = "windows-cpp";

    const ProjectWriteResult w = ProjectControl::writeFiles(s, m_tmp->path());
    QVERIFY(!w.ok);
    QVERIFY(w.error.contains("msvc-qt6"));
    QVERIFY(!ProjectControl::hasCompose(m_tmp->path()));
}

QTEST_MAIN(TstProjectControl)
#include "tst_project_control.moc"
