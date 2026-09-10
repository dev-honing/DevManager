#include "snapshot/snapshot_retention.h"

#include <QtTest>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>

using namespace dm;

class TstSnapshotRetention : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void keepLastPrunesOlderButNeverTheNewest();
    void keepDaysKeepsRecentAndPreRestoreIsNeverListed();
    void applyRemovesExactlyThePlannedDirs();

private:
    QTemporaryDir* m_tmp = nullptr;
    QString m_backups;

    // a snapshot dir with a manifest whose capturedAt is `ageDays` old
    void makeSnap(const QString& stamp, int ageDays)
    {
        const QString d = m_backups + "/" + stamp;
        QDir().mkpath(d);
        const QString cap =
            QDateTime::currentDateTime().addDays(-ageDays).toString(Qt::ISODateWithMs);
        QFile f(d + "/manifest.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QString("{\"stamp\":\"%1\",\"capturedAt\":\"%2\",\"components\":[]}")
                    .arg(stamp, cap).toUtf8());
        f.close();
        QFile p(d + "/payload.bin");
        QVERIFY(p.open(QIODevice::WriteOnly));
        p.write(QByteArray(1000, 'x'));
    }
};

void TstSnapshotRetention::init()
{
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    m_backups = m_tmp->path() + "/backups";
    QDir().mkpath(m_backups);
}

void TstSnapshotRetention::cleanup()
{
    delete m_tmp;
    m_tmp = nullptr;
}

void TstSnapshotRetention::keepLastPrunesOlderButNeverTheNewest()
{
    makeSnap("2026-01-01T000000", 40);
    makeSnap("2026-02-01T000000", 30);
    makeSnap("2026-03-01T000000", 20);
    makeSnap("2026-04-01T000000", 10);

    RetentionPolicy p;
    p.keepLast = 2;
    const PrunePlan plan = SnapshotRetention::plan(m_backups, p);

    QCOMPARE(plan.keep.size(), 2);
    QCOMPARE(plan.prune.size(), 2);
    QVERIFY(plan.keep.contains("2026-04-01T000000"));   // newest always kept
    QVERIFY(plan.keep.contains("2026-03-01T000000"));
    QVERIFY(plan.pruneBytes > 0);
}

void TstSnapshotRetention::keepDaysKeepsRecentAndPreRestoreIsNeverListed()
{
    makeSnap("2026-01-01T000000", 40);
    makeSnap("2026-04-01T000000", 3);
    // a transient safety folder that retention must ignore entirely
    QDir().mkpath(m_backups + "/pre-restore-2026-01-01T000000");
    QFile f(m_backups + "/pre-restore-2026-01-01T000000/manifest.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"components\":[]}");
    f.close();

    RetentionPolicy p;
    p.keepDays = 7;
    const PrunePlan plan = SnapshotRetention::plan(m_backups, p);

    QCOMPARE(plan.keep, QStringList{"2026-04-01T000000"});
    QCOMPARE(plan.prune.size(), 1);
    QVERIFY(plan.prune.at(0).endsWith("2026-01-01T000000"));
    for (const QString& d : plan.prune)
        QVERIFY(!d.contains("pre-restore"));
}

void TstSnapshotRetention::applyRemovesExactlyThePlannedDirs()
{
    makeSnap("2026-01-01T000000", 40);
    makeSnap("2026-02-01T000000", 30);
    makeSnap("2026-03-01T000000", 5);

    RetentionPolicy p;
    p.keepLast = 1;
    const PrunePlan plan = SnapshotRetention::plan(m_backups, p);
    QCOMPARE(plan.prune.size(), 2);

    const PruneResult r = SnapshotRetention::apply(plan);
    QVERIFY2(r.ok, r.errors.join("; ").toUtf8());
    QCOMPARE(r.removed, 2);
    QVERIFY(r.bytes > 0);

    QVERIFY(QFileInfo::exists(m_backups + "/2026-03-01T000000"));
    QVERIFY(!QFileInfo::exists(m_backups + "/2026-01-01T000000"));
    QVERIFY(!QFileInfo::exists(m_backups + "/2026-02-01T000000"));
}

QTEST_MAIN(TstSnapshotRetention)
#include "tst_snapshot_retention.moc"
