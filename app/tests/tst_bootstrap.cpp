#include "bootstrap.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstBootstrap : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void listsMissingToolsAndCarriesTheInstallHint();

private:
    QByteArray m_savedProfile;
    QTemporaryDir* m_tmp = nullptr;
    QString m_cfgDir;
};

void TstBootstrap::init()
{
    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_cfgDir = m_tmp->path() + "/cfg";

    QJsonObject cfg{
        {"tools", QJsonArray{
                      QJsonObject{{"id", "dm-missing-with-hint"}, {"category", "ai"},
                                  {"install", "npm install -g dm-thing"}},
                      QJsonObject{{"id", "dm-missing-no-hint"}, {"category", "ai"}}}},
        {"services", QJsonArray{}},
        {"packageManagers", QJsonArray{}},
        {"envInclude", QJsonArray{}},
        {"qtSearchPaths", QJsonArray{}},
        {"backupRoots", QJsonArray{}},
    };
    QDir().mkpath(m_cfgDir);
    QFile c(m_cfgDir + "/scan.json");
    QVERIFY(c.open(QIODevice::WriteOnly));
    c.write(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    c.close();
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstBootstrap::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstBootstrap::listsMissingToolsAndCarriesTheInstallHint()
{
    const BootstrapPlan p = Bootstrap::plan();

    QCOMPARE(p.missing, 2);
    QCOMPARE(p.withHint, 1);

    QString hintCmd, noHintTool;
    for (const BootstrapStep& s : p.steps) {
        if (s.haveHint) hintCmd = s.command;
        else noHintTool = s.tool;
    }
    QCOMPARE(hintCmd, QStringLiteral("npm install -g dm-thing"));
    QCOMPARE(noHintTool, QStringLiteral("dm-missing-no-hint"));
}

QTEST_MAIN(TstBootstrap)
#include "tst_bootstrap.moc"
