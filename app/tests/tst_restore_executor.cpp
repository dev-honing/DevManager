#include "fs_ops.h"
#include "json_io.h"
#include "process_runner.h"
#include "snapshot/restore_executor.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_preview.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstRestoreExecutor : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void restoresPreservesAndRelinks();
    void rollsBackWhenABackupIsMissing();
    void relinkSkipsLocationsOutsideRestoredScope();

private:
    QByteArray m_savedProfile;
    QTemporaryDir* m_tmp = nullptr;
    QString m_home, m_canon, m_cfgDir, m_backups;
    bool m_haveJunction = false;

    static void put(const QString& p, const QByteArray& d)
    {
        QDir().mkpath(QFileInfo(p).absolutePath());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(d);
    }
    SnapshotResult buildSnapshot();
};

void TstRestoreExecutor::init()
{
    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());

    m_home = m_tmp->path() + "/home";
    m_canon = m_tmp->path() + "/canon/gpt-image";
    m_cfgDir = m_tmp->path() + "/cfg";
    m_backups = m_tmp->path() + "/backups";

    put(m_home + "/.claude/settings.json", "{\"orig\":1}");
    put(m_home + "/.claude/skills/foo/SKILL.md", "ORIG");
    put(m_canon + "/SKILL.md", "canon");
    QDir().mkpath(m_home + "/.agents/skills");

#ifdef Q_OS_WIN
    const ProcessResult mk = ProcessRunner::run(
        "cmd",
        {"/c", "mklink", "/J",
         QDir::toNativeSeparators(m_home + "/.agents/skills/gpt-image"),
         QDir::toNativeSeparators(m_canon)},
        5000);
    m_haveJunction = mk.ok();
#endif

    qputenv("USERPROFILE", m_home.toUtf8());

    QJsonObject cfg{
        {"backupRoots", QJsonArray{m_home + "/.claude", m_home + "/.agents"}},
    };
    put(m_cfgDir + "/scan.json",
        QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstRestoreExecutor::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

SnapshotResult TstRestoreExecutor::buildSnapshot()
{
    const SnapshotPreview pv = SnapshotPlanner::compute();

    // hand-built inventory so the relink phase has the skill link topology
    QJsonObject link;
    link["isLink"] = true;
    link["linkType"] = "Junction";
    link["target"] = QDir::toNativeSeparators(m_canon);

    QJsonObject loc;
    loc["host"] = "agents";
    loc["path"] = QDir::toNativeSeparators(m_home + "/.agents/skills/gpt-image");
    loc["link"] = link;

    // a second link whose location is NOT under any restored root (.gemini is
    // not a backupRoot here) -- the relink phase must leave it alone
    QJsonObject outLoc;
    outLoc["host"] = "gemini";
    outLoc["path"] = QDir::toNativeSeparators(m_home + "/.gemini/skills/gpt-image");
    outLoc["link"] = link;

    QJsonObject skill;
    skill["name"] = "gpt-image";
    skill["locations"] = QJsonArray{loc, outLoc};

    QJsonObject discovery;
    discovery["skills"] = QJsonArray{skill};
    QJsonObject inv;
    inv["discovery"] = discovery;

    return SnapshotExecutor::run(pv, m_backups, inv);
}

void TstRestoreExecutor::restoresPreservesAndRelinks()
{
    const SnapshotResult sr = buildSnapshot();
    QVERIFY2(sr.ok, sr.errors.join("; ").toUtf8());

    // mutate the live tree
    QVERIFY(QFile::remove(m_home + "/.claude/settings.json"));
    put(m_home + "/.claude/skills/foo/SKILL.md", "MUTATED");

    RestoreOptions opts;
    opts.recreateLinks = true;
    const RestoreResult rr = RestoreExecutor::run(sr.snapshotDir, opts);

    QVERIFY2(rr.ok, rr.errors.join("; ").toUtf8());
    QCOMPARE(rr.state, RestoreState::Done);

    // restored
    QVERIFY(QFileInfo::exists(m_home + "/.claude/settings.json"));
    QFile md(m_home + "/.claude/skills/foo/SKILL.md");
    QVERIFY(md.open(QIODevice::ReadOnly));
    QCOMPARE(md.readAll(), QByteArray("ORIG"));

    // pre-restore backup kept, with the mutated version inside
    QVERIFY(QFileInfo::exists(rr.preRestoreDir));
    QVERIFY(!QDir(rr.preRestoreDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());

    if (m_haveJunction) {
        const QFileInfo li(m_home + "/.agents/skills/gpt-image");
        QVERIFY2(li.isJunction(), "gpt-image should be a junction again");
        QVERIFY(rr.linkResults.join(";").contains("recreated"));
    }
}

void TstRestoreExecutor::rollsBackWhenABackupIsMissing()
{
    const SnapshotResult sr = buildSnapshot();
    QVERIFY(sr.ok);

    // corrupt the snapshot: delete a backed-up component's payload
    QStringList errs;
    QVERIFY(fs::removeTree(sr.snapshotDir + "/.claude/settings.json", &errs));

    // mutate live so we can tell rollback restored the pre-restore copy
    put(m_home + "/.claude/settings.json", "{\"mutated\":1}");

    const RestoreResult rr = RestoreExecutor::run(sr.snapshotDir, RestoreOptions{});

    QVERIFY(!rr.ok);
    QCOMPARE(rr.state, RestoreState::RolledBack);
    QVERIFY(rr.rollbackErrors.isEmpty());

    // the live tree is back to the mutated (pre-restore) state, not empty/half
    QFile f(m_home + "/.claude/settings.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("{\"mutated\":1}"));
}

void TstRestoreExecutor::relinkSkipsLocationsOutsideRestoredScope()
{
    const SnapshotResult sr = buildSnapshot();
    QVERIFY2(sr.ok, sr.errors.join("; ").toUtf8());

    RestoreOptions opts;
    opts.recreateLinks = true;
    const RestoreResult rr = RestoreExecutor::run(sr.snapshotDir, opts);
    QVERIFY2(rr.ok, rr.errors.join("; ").toUtf8());

    const QString joined = rr.linkResults.join("\n");
    QVERIFY2(joined.contains(".gemini") && joined.contains("outside restored scope"),
             joined.toUtf8());
    // nothing was created under the un-restored root
    QVERIFY(!QFileInfo::exists(m_home + "/.gemini/skills/gpt-image"));
}

QTEST_MAIN(TstRestoreExecutor)
#include "tst_restore_executor.moc"
