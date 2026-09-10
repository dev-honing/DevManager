#include "health_check.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstHealthCheck : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void flagsMissingToolAndAbsentBackupRoot();
    void profileFiltersToItsTools();

private:
    QByteArray m_savedProfile;
    QTemporaryDir* m_tmp = nullptr;
    QString m_cfgDir;
};

void TstHealthCheck::init()
{
    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_cfgDir = m_tmp->path() + "/cfg";

    const QString present = m_tmp->path() + "/present";
    const QString absent = m_tmp->path() + "/absent";
    QDir().mkpath(present);

    QJsonObject cfg{
        {"tools", QJsonArray{
                      QJsonObject{{"id", "dm-nonexistent-tool-xyz"}, {"category", "ai"}},
                      QJsonObject{{"id", "dm-other-missing-tool"}, {"category", "ai"}}}},
        {"services", QJsonArray{
                         QJsonObject{{"id", "nothing"}, {"name", "Nothing"}, {"port", 59997}}}},
        {"packageManagers", QJsonArray{}},
        {"envInclude", QJsonArray{}},
        {"qtSearchPaths", QJsonArray{}},
        {"backupRoots", QJsonArray{present, absent}},
        {"hostProfiles", QJsonObject{
             {"solo", QJsonObject{{"tools", QJsonArray{"dm-nonexistent-tool-xyz"}}}}}},
    };
    QDir().mkpath(m_cfgDir);
    QFile c(m_cfgDir + "/scan.json");
    QVERIFY(c.open(QIODevice::WriteOnly));
    c.write(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    c.close();
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstHealthCheck::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstHealthCheck::flagsMissingToolAndAbsentBackupRoot()
{
    const HealthReport h = HealthCheck::run();

    QVERIFY(!h.ok);              // the missing tool is a fail
    QVERIFY(h.failCount >= 1);

    int toolFail = 0, rootOk = 0, rootWarn = 0, svcSeen = 0;
    for (const auto& it : h.items) {
        if (it.group == "tool" && it.status == "fail") toolFail++;
        if (it.group == "backup-root" && it.status == "ok") rootOk++;
        if (it.group == "backup-root" && it.status == "warn") rootWarn++;
        if (it.group == "service") svcSeen++;
    }
    QCOMPARE(toolFail, 2);
    QCOMPARE(rootOk, 1);
    QCOMPARE(rootWarn, 1);
    QVERIFY(svcSeen >= 1);       // the unreachable service is reported (warn/fail)
}

void TstHealthCheck::profileFiltersToItsTools()
{
    const HealthReport h = HealthCheck::run("solo");   // profile lists only one tool
    int toolItems = 0;
    for (const auto& it : h.items)
        if (it.group == "tool")
            toolItems++;
    QCOMPARE(toolItems, 1);
    // backup roots / services are still checked
    bool sawRoot = false, sawSvc = false;
    for (const auto& it : h.items) {
        if (it.group == "backup-root") sawRoot = true;
        if (it.group == "service") sawSvc = true;
    }
    QVERIFY(sawRoot);
    QVERIFY(sawSvc);
}

QTEST_MAIN(TstHealthCheck)
#include "tst_health_check.moc"
