#include "snapshot/snapshot_bundle.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_preview.h"
#include "snapshot/snapshot_verify.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

using namespace dm;

class TstSnapshotBundle : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void packThenUnpackRoundTrips();
    void packRejectsNonSnapshotDir();

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
};

void TstSnapshotBundle::init()
{
    if (QStandardPaths::findExecutable("tar").isEmpty())
        QSKIP("system tar not available");

    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_home = m_tmp->path() + "/home";
    m_cfgDir = m_tmp->path() + "/cfg";
    m_backups = m_tmp->path() + "/backups";

    put(m_home + "/.claude/settings.json", "{\"v\":1}");
    put(m_home + "/.claude/skills/foo/SKILL.md", "foo skill body");

    qputenv("USERPROFILE", m_home.toUtf8());
    QJsonObject cfg{
        {"backupRoots", QJsonArray{m_home + "/.claude"}},
        {"snapshot", QJsonObject{{"rootDefaults", QJsonObject{{"*", "backup"}}}}},
    };
    put(m_cfgDir + "/scan.json", QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstSnapshotBundle::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstSnapshotBundle::packThenUnpackRoundTrips()
{
    const SnapshotResult sr =
        SnapshotExecutor::run(SnapshotPlanner::compute(), m_backups);
    QVERIFY2(sr.ok, sr.errors.join("; ").toUtf8());

    const BundleResult packed = SnapshotBundle::pack(sr.snapshotDir);
    QVERIFY2(packed.ok, packed.error.toUtf8());
    QVERIFY(QFileInfo::exists(packed.path));
    QVERIFY(packed.bytes > 0);

    const QString dest = m_tmp->path() + "/onNewPC";
    const BundleResult out = SnapshotBundle::unpack(packed.path, dest);
    QVERIFY2(out.ok, out.error.toUtf8());

    QVERIFY(QFileInfo::exists(out.path + "/manifest.json"));
    QFile md(out.path + "/.claude/skills/foo/SKILL.md");
    QVERIFY(md.open(QIODevice::ReadOnly));
    QCOMPARE(md.readAll(), QByteArray("foo skill body"));

    // digests survive the round trip
    const VerifyResult v = SnapshotVerify::check(out.path);
    QVERIFY2(v.ok, "unpacked snapshot must still verify");
    QCOMPARE(v.badCount, 0);
}

void TstSnapshotBundle::packRejectsNonSnapshotDir()
{
    QDir().mkpath(m_tmp->path() + "/random");
    const BundleResult r = SnapshotBundle::pack(m_tmp->path() + "/random");
    QVERIFY(!r.ok);
    QVERIFY(!r.error.isEmpty());
}

QTEST_MAIN(TstSnapshotBundle)
#include "tst_snapshot_bundle.moc"
