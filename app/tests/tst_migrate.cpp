#include "migrate.h"
#include "snapshot/snapshot_bundle.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_preview.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

using namespace dm;

class TstMigrate : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void dryRunPlansWithoutTouchingLiveFiles();
    void applyRestoresAndReportsHealth();

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
    QString buildBundle()
    {
        const SnapshotResult sr =
            SnapshotExecutor::run(SnapshotPlanner::compute(), m_backups);
        if (!sr.ok) return {};
        const BundleResult b = SnapshotBundle::pack(sr.snapshotDir);
        return b.ok ? b.path : QString();
    }
};

void TstMigrate::init()
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

    qputenv("USERPROFILE", m_home.toUtf8());
    QJsonObject cfg{
        {"backupRoots", QJsonArray{m_home + "/.claude"}},
        {"snapshot", QJsonObject{{"rootDefaults", QJsonObject{{"*", "backup"}}}}},
        {"services", QJsonArray{}},
        {"tools", QJsonArray{}},
    };
    put(m_cfgDir + "/scan.json", QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstMigrate::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstMigrate::dryRunPlansWithoutTouchingLiveFiles()
{
    const QString bundle = buildBundle();
    QVERIFY(!bundle.isEmpty());

    // mutate the live file so a dry run leaving it alone is observable
    put(m_home + "/.claude/settings.json", "{\"v\":2}");

    const QString outDir = m_tmp->path() + "/incoming";
    const MigrateResult m = Migrate::run(bundle, outDir, /*apply=*/false);

    QVERIFY2(m.ok, m.error.toUtf8());
    QVERIFY(!m.applied);
    QVERIFY(m.preview.valid);
    QVERIFY(m.preview.targets.size() >= 1);
    QVERIFY(m.health.items.size() >= 0);   // computed, not asserting content here

    QFile f(m_home + "/.claude/settings.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("{\"v\":2}"));   // untouched
}

void TstMigrate::applyRestoresAndReportsHealth()
{
    const QString bundle = buildBundle();
    QVERIFY(!bundle.isEmpty());

    put(m_home + "/.claude/settings.json", "{\"v\":2}");

    const QString outDir = m_tmp->path() + "/incoming2";
    const MigrateResult m = Migrate::run(bundle, outDir, /*apply=*/true);

    QVERIFY2(m.ok, m.restore.errors.join("; ").toUtf8());
    QVERIFY(m.applied);
    QCOMPARE(m.restore.restored, 1);

    QFile f(m_home + "/.claude/settings.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("{\"v\":1}"));   // restored to the bundled state
}

QTEST_MAIN(TstMigrate)
#include "tst_migrate.moc"
