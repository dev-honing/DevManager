#include "fs_ops.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_preview.h"
#include "snapshot/snapshot_verify.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstSnapshotVerify : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void cleanSnapshotVerifiesOk();
    void tamperedFileIsAMismatch();
    void deletedComponentIsMissing();

private:
    QByteArray m_savedProfile;
    QTemporaryDir* m_tmp = nullptr;
    QString m_home, m_cfgDir, m_backups;

    static void put(const QString& p, const QByteArray& d)
    {
        QDir().mkpath(QFileInfo(p).absolutePath());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(d);
    }
    QString buildSnapshot();
};

void TstSnapshotVerify::init()
{
    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_home = m_tmp->path() + "/home";
    m_cfgDir = m_tmp->path() + "/cfg";
    m_backups = m_tmp->path() + "/backups";

    put(m_home + "/.claude/settings.json", "{\"orig\":1}");
    put(m_home + "/.claude/skills/foo/SKILL.md", "hello skill");

    qputenv("USERPROFILE", m_home.toUtf8());
    QJsonObject cfg{
        {"backupRoots", QJsonArray{m_home + "/.claude"}},
        {"snapshot", QJsonObject{{"rootDefaults", QJsonObject{{"*", "backup"}}}}},
    };
    put(m_cfgDir + "/scan.json", QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstSnapshotVerify::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

QString TstSnapshotVerify::buildSnapshot()
{
    const SnapshotPreview pv = SnapshotPlanner::compute();
    const SnapshotResult sr = SnapshotExecutor::run(pv, m_backups);
    return sr.ok ? sr.snapshotDir : QString();
}

void TstSnapshotVerify::cleanSnapshotVerifiesOk()
{
    const QString dir = buildSnapshot();
    QVERIFY(!dir.isEmpty());

    const VerifyResult v = SnapshotVerify::check(dir);
    QVERIFY2(v.ok, "a freshly written snapshot must verify");
    QVERIFY(v.error.isEmpty());
    QCOMPARE(v.badCount, 0);
    QVERIFY(v.okCount > 0);
}

void TstSnapshotVerify::tamperedFileIsAMismatch()
{
    const QString dir = buildSnapshot();
    QVERIFY(!dir.isEmpty());

    // change a byte inside a backed-up component
    QFile f(dir + "/.claude/settings.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"orig\":2}");
    f.close();

    const VerifyResult v = SnapshotVerify::check(dir);
    QVERIFY(!v.ok);
    QVERIFY(v.badCount >= 1);
    bool sawMismatch = false;
    for (const auto& c : v.components)
        if (c.status == "mismatch")
            sawMismatch = true;
    QVERIFY(sawMismatch);
}

void TstSnapshotVerify::deletedComponentIsMissing()
{
    const QString dir = buildSnapshot();
    QVERIFY(!dir.isEmpty());

    QStringList errs;
    QVERIFY(fs::removeTree(dir + "/.claude/skills", &errs));

    const VerifyResult v = SnapshotVerify::check(dir);
    QVERIFY(!v.ok);
    bool sawMissing = false;
    for (const auto& c : v.components)
        if (c.status == "missing")
            sawMissing = true;
    QVERIFY(sawMissing);
}

QTEST_MAIN(TstSnapshotVerify)
#include "tst_snapshot_verify.moc"
