#include "json_io.h"
#include "snapshot/snapshot_coordinator.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

// The blocker services in the fixture config run `cmd /c mkdir <marker>` for
// stop and start, so the test can see that the coordinator stopped them before
// the snapshot and started them again afterwards.
class TstSnapshotCoordinator : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void stopsBlockersTakesConsistentSnapshotThenRestarts();

private:
    QByteArray m_savedProfile;
    QTemporaryDir* m_tmp = nullptr;
    QString m_home, m_cfgDir, m_backups, m_marks;
};

void TstSnapshotCoordinator::init()
{
#ifndef Q_OS_WIN
    QSKIP("cmd-based lifecycle fixture is Windows-only");
#endif
    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());

    m_home = m_tmp->path() + "/home";
    m_cfgDir = m_tmp->path() + "/cfg";
    m_backups = m_tmp->path() + "/backups";
    m_marks = m_tmp->path() + "/marks";

    QDir().mkpath(m_home + "/.claude");
    QFile f(m_home + "/.claude/settings.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"v\":1}");
    f.close();

    qputenv("USERPROFILE", m_home.toUtf8());

    const QString stopMark = QDir::toNativeSeparators(m_marks + "/hr-stopped");
    const QString startMark = QDir::toNativeSeparators(m_marks + "/hr-started");
    QJsonObject lc{
        {"exe", "cmd"},
        {"status", QJsonArray{"/c", "echo", "ok"}},
        {"stop", QJsonArray{"/c", "mkdir", stopMark}},
        {"start", QJsonArray{"/c", "mkdir", startMark}},
        {"startDetached", false},
    };
    QJsonObject headroom{
        {"id", "headroom"}, {"name", "Headroom"}, {"snapshotBlocker", true},
        {"lifecycle", lc},
    };
    QJsonObject cfg{
        {"backupRoots", QJsonArray{m_home + "/.claude"}},
        {"services", QJsonArray{headroom}},
        {"snapshot", QJsonObject{{"rootDefaults", QJsonObject{{"*", "backup"}}}}},
    };
    QDir().mkpath(m_cfgDir);
    QFile c(m_cfgDir + "/scan.json");
    QVERIFY(c.open(QIODevice::WriteOnly));
    c.write(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    c.close();
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstSnapshotCoordinator::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstSnapshotCoordinator::stopsBlockersTakesConsistentSnapshotThenRestarts()
{
    QList<ServiceState> services;
    ServiceState hr;
    hr.id = "headroom";
    hr.name = "Headroom";
    hr.level = ServiceState::Running;
    services << hr;

    const ConsistentSnapshotResult r =
        SnapshotCoordinator::runConsistent(services, m_backups);

    QVERIFY2(r.snapshot.ok, r.snapshot.errors.join("; ").toUtf8());
    QCOMPARE(r.stopped, QStringList{"headroom"});
    QCOMPARE(r.restarted, QStringList{"headroom"});
    QVERIFY(r.servicesRestored);
    QVERIFY2(r.serviceErrors.isEmpty(), r.serviceErrors.join("; ").toUtf8());

    // both lifecycle hooks actually fired
    QVERIFY(QFileInfo::exists(m_marks + "/hr-stopped"));
    QVERIFY(QFileInfo::exists(m_marks + "/hr-started"));

    // the snapshot was planned with the blocker down => consistent
    const QJsonObject m =
        json::read(QDir(r.snapshot.snapshotDir).filePath("manifest.json"));
    QCOMPARE(m.value("consistentSnapshot").toBool(), true);
    QVERIFY(m.value("serviceBlockers").toArray().isEmpty());
}

QTEST_MAIN(TstSnapshotCoordinator)
#include "tst_snapshot_coordinator.moc"
