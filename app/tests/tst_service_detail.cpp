#include "service/service_detail.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstServiceDetail : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void reportsHealthAndEnvWiring();

private:
    QTemporaryDir* m_tmp = nullptr;
    QString m_cfgDir;
};

void TstServiceDetail::init()
{
#ifndef Q_OS_WIN
    QSKIP("cmd-based lifecycle fixture is Windows-only");
#endif
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_cfgDir = m_tmp->path() + "/cfg";
    QDir().mkpath(m_cfgDir);

    QJsonObject withHealth{
        {"id", "svc-a"}, {"name", "Svc A"}, {"port", 1234},
        {"envIndicatorVar", "TEST_URL"},
        {"lifecycle", QJsonObject{{"exe", "cmd"},
                                  {"status", QJsonArray{"/c", "echo", "Healthy: yes"}}}},
    };
    QJsonObject bare{{"id", "svc-b"}, {"name", "Svc B"}, {"port", 9999}};
    QJsonObject cfg{{"services", QJsonArray{withHealth, bare}}};

    QFile f(m_cfgDir + "/scan.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    f.close();
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstServiceDetail::cleanup()
{
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstServiceDetail::reportsHealthAndEnvWiring()
{
    QList<ServiceState> services;
    ServiceState a;
    a.id = "svc-a";
    a.name = "Svc A";
    a.level = ServiceState::Running;
    services << a;
    // svc-b not probed -> stays "not running" by default

    QMap<QString, QString> env{{"TEST_URL", "http://127.0.0.1:1234/v1"}};

    const QList<ServiceDetail> details = ServiceDetailCheck::run(services, env);
    QCOMPARE(details.size(), 2);

    const ServiceDetail& a2 = details.at(0);
    QCOMPARE(a2.id, QStringLiteral("svc-a"));
    QVERIFY(a2.running);
    QCOMPARE(a2.health, QStringLiteral("healthy"));
    QVERIFY(a2.envWired);
    QCOMPARE(a2.envVar, QStringLiteral("TEST_URL"));

    const ServiceDetail& b = details.at(1);
    QCOMPARE(b.id, QStringLiteral("svc-b"));
    QVERIFY(!b.running);
    QVERIFY(b.health.isEmpty());     // no lifecycle -> no status command run
    QVERIFY(!b.envWired);            // no envIndicatorVar configured
}

QTEST_MAIN(TstServiceDetail)
#include "tst_service_detail.moc"
