#include "snapshot/snapshot_diff.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_preview.h"

#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace dm;

class TstSnapshotDiff : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void reportsAddedRemovedAndChanged();
    void identicalSnapshotsHaveNoDeltas();

private:
    QByteArray m_savedProfile;
    QTemporaryDir* m_tmp = nullptr;
    QString m_home, m_cfgDir;
    int m_run = 0;

    static void put(const QString& p, const QByteArray& d)
    {
        QDir().mkpath(QFileInfo(p).absolutePath());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(d);
    }
    QString snapshot()   // each call writes to its own base dir
    {
        const QString base = m_tmp->path() + QString("/backups%1").arg(m_run++);
        const SnapshotResult sr =
            SnapshotExecutor::run(SnapshotPlanner::compute(), base);
        return sr.ok ? sr.snapshotDir : QString();
    }
};

void TstSnapshotDiff::init()
{
    m_savedProfile = qgetenv("USERPROFILE");
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_home = m_tmp->path() + "/home";
    m_cfgDir = m_tmp->path() + "/cfg";
    m_run = 0;

    put(m_home + "/.claude/settings.json", "{\"v\":1}");
    put(m_home + "/.claude/CLAUDE.md", "rules");
    put(m_home + "/.claude/skills/foo/SKILL.md", "foo");

    qputenv("USERPROFILE", m_home.toUtf8());
    QJsonObject cfg{
        {"backupRoots", QJsonArray{m_home + "/.claude"}},
        {"snapshot", QJsonObject{{"rootDefaults", QJsonObject{{"*", "backup"}}}}},
    };
    put(m_cfgDir + "/scan.json", QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    qputenv("DEVMANAGER_CONFIG_DIR", m_cfgDir.toUtf8());
}

void TstSnapshotDiff::cleanup()
{
    qputenv("USERPROFILE", m_savedProfile);
    qunsetenv("DEVMANAGER_CONFIG_DIR");
    delete m_tmp;
    m_tmp = nullptr;
}

void TstSnapshotDiff::reportsAddedRemovedAndChanged()
{
    const QString a = snapshot();
    QVERIFY(!a.isEmpty());

    put(m_home + "/.claude/settings.json", "{\"v\":2}");        // changed
    QVERIFY(QFile::remove(m_home + "/.claude/CLAUDE.md"));       // removed
    put(m_home + "/.claude/hooks.json", "{}");                  // added

    const QString b = snapshot();
    QVERIFY(!b.isEmpty());

    const DiffResult d = SnapshotDiff::compare(a, b);
    QVERIFY2(d.ok, d.error.toUtf8());
    QCOMPARE(d.added, 1);
    QCOMPARE(d.removed, 1);
    QCOMPARE(d.changed, 1);

    QMap<QString, QString> byChange;
    for (const auto& x : d.deltas)
        byChange.insert(x.change, x.name);
    QVERIFY(byChange.value("added").contains("hooks.json"));
    QVERIFY(byChange.value("removed").contains("CLAUDE.md"));
    QVERIFY(byChange.value("changed").contains("settings.json"));
}

void TstSnapshotDiff::identicalSnapshotsHaveNoDeltas()
{
    const QString a = snapshot();
    const QString b = snapshot();
    QVERIFY(!a.isEmpty() && !b.isEmpty());

    const DiffResult d = SnapshotDiff::compare(a, b);
    QVERIFY(d.ok);
    QCOMPARE(d.deltas.size(), 0);
    QCOMPARE(d.added + d.removed + d.changed, 0);
}

QTEST_MAIN(TstSnapshotDiff)
#include "tst_snapshot_diff.moc"
