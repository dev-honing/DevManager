#include "project/project_env.h"

#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

using namespace dm;

class TstProjectEnv : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void resolvesTypeFromConfigAndBuildsCompose();
    void hostProfileTypeHasNoCompose();
    void unknownTypeFallsBackToBaseImage();

private:
    QTemporaryDir* m_tmp = nullptr;
    QString m_cfgDir;
};

void TstProjectEnv::init()
{
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_cfgDir = m_tmp->path() + "/cfg";
    QDir().mkpath(m_cfgDir);
    QFile f(m_cfgDir + "/project-types.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(R"({
      "cpp":         { "image": "ai-dev-cpp:0.1", "docker": true, "workspace": "/workspace",
                       "volumes": ["workspace", "claude", "codex", "deps"] },
      "windows-cpp": { "docker": false, "hostProfile": "msvc-qt6" }
    })");
    f.close();
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstProjectEnv::cleanup()
{
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstProjectEnv::resolvesTypeFromConfigAndBuildsCompose()
{
    const ProjectSpec s = ProjectEnv::resolve("myproj", "cpp");
    QVERIFY(s.valid);
    QVERIFY(s.docker);
    QCOMPARE(s.image, QStringLiteral("ai-dev-cpp:0.1"));

    const QString yaml = ProjectEnv::composeYaml(s);
    QVERIFY(yaml.contains("name: myproj"));
    QVERIFY(yaml.contains("image: ai-dev-cpp:0.1"));
    QVERIFY(yaml.contains("- ..:/workspace"));                 // project bind
    QVERIFY(yaml.contains("- claude:/home/node/.claude"));
    QVERIFY(yaml.contains("- codex:/home/node/.codex"));
    QVERIFY(yaml.contains("host.docker.internal:host-gateway"));
    QVERIFY(yaml.contains("volumes:\n  claude:\n  codex:\n  deps:"));

    const QString dc = ProjectEnv::devcontainerJson(s);
    QVERIFY(dc.contains("\"dockerComposeFile\": \"../.devmanager/docker-compose.yml\""));
    QVERIFY(dc.contains("\"service\": \"dev\""));
    QVERIFY(dc.contains("\"workspaceFolder\": \"/workspace\""));
}

void TstProjectEnv::hostProfileTypeHasNoCompose()
{
    const ProjectSpec s = ProjectEnv::resolve("native", "windows-cpp");
    QVERIFY(s.valid);
    QVERIFY(!s.docker);
    QCOMPARE(s.hostProfile, QStringLiteral("msvc-qt6"));
    QVERIFY(ProjectEnv::composeYaml(s).isEmpty());
    QVERIFY(ProjectEnv::devcontainerJson(s).isEmpty());
}

void TstProjectEnv::unknownTypeFallsBackToBaseImage()
{
    const ProjectSpec s = ProjectEnv::resolve("x", "rust-or-whatever");
    QVERIFY(s.valid);
    QVERIFY(s.docker);
    QCOMPARE(s.image, QStringLiteral("ai-dev-base:0.1"));
}

QTEST_MAIN(TstProjectEnv)
#include "tst_project_env.moc"
