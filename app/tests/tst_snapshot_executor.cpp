#include "snapshot/snapshot_executor.h"
#include "json_io.h"
#include "process_runner.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstSnapshotExecutor : public QObject {
    Q_OBJECT
private slots:
    void copiesBackupEntriesAndRecordsLinks();
};

static void put(const QString& p, const QByteArray& data = "x")
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void TstSnapshotExecutor::copiesBackupEntriesAndRecordsLinks()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // fixture source tree
    const QString src = tmp.path() + "/.claude";
    put(src + "/settings.json", "{\"a\":1}");
    put(src + "/skills/foo/SKILL.md", "s");
    put(src + "/cache/junk.bin", QByteArray(4096, 'z'));   // should be excluded by caller

    // a junction under a backed-up dir -> must be recorded, not walked
    const QString canon = tmp.path() + "/canon";
    put(canon + "/inner.txt", "c");
    const QString linkPath = QDir::toNativeSeparators(src + "/skills/linked");
#ifdef Q_OS_WIN
    const ProcessResult mk = ProcessRunner::run(
        "cmd", {"/c", "mklink", "/J", linkPath, QDir::toNativeSeparators(canon)}, 5000);
    const bool haveJunction = mk.ok();
#else
    const bool haveJunction = false;
#endif

    // hand-built preview: settings.json + skills  are Backup, cache is Exclude
    SnapshotPreview pv;
    pv.valid = true;
    auto mk_art = [&](const QString& name, SnapshotPolicy pol) {
        PlannedArtifact a;
        a.rootId = ".claude";
        a.name = name;
        a.path = QDir::toNativeSeparators(src + "/" + name);
        a.policy = pol;
        return a;
    };
    pv.artifacts << mk_art("settings.json", SnapshotPolicy::Backup)
                 << mk_art("skills", SnapshotPolicy::Backup)
                 << mk_art("cache", SnapshotPolicy::Exclude);
    pv.inventoryOnlyCount = 0;
    pv.regenerateCount = 0;
    pv.excludeCount = 1;

    const QString destBase = tmp.path() + "/backups";
    const SnapshotResult r =
        SnapshotExecutor::run(pv, destBase, QJsonObject{{"schemaVersion", 5}});

    QVERIFY2(r.ok, r.errors.join("; ").toUtf8());
    QVERIFY(QFileInfo::exists(r.snapshotDir + "/manifest.json"));
    QVERIFY(QFileInfo::exists(r.snapshotDir + "/inventory.json"));

    // Backup entries copied
    QVERIFY(QFileInfo::exists(r.snapshotDir + "/.claude/settings.json"));
    QVERIFY(QFileInfo::exists(r.snapshotDir + "/.claude/skills/foo/SKILL.md"));
    // Exclude entry not copied
    QVERIFY(!QFileInfo::exists(r.snapshotDir + "/.claude/cache"));

    if (haveJunction) {
        // the junction dir itself is NOT recreated as a real folder, and its
        // target contents are NOT copied
        QVERIFY(!QFileInfo::exists(r.snapshotDir + "/.claude/skills/linked/inner.txt"));
        QVERIFY(!r.linkNotes.isEmpty());
    }

    const QJsonObject m = json::read(r.snapshotDir + "/manifest.json");
    QCOMPARE(m.value("schemaVersion").toInt(), 5);
    QVERIFY(m.value("components").toArray().size() >= 2);
}

QTEST_MAIN(TstSnapshotExecutor)
#include "tst_snapshot_executor.moc"
