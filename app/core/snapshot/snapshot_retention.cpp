#include "snapshot/snapshot_retention.h"

#include "fs_ops.h"
#include "snapshot/snapshot_index.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace dm {

namespace {

qint64 dirBytes(const QString& path)
{
    qint64 n = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        n += it.fileInfo().size();
    }
    return n;
}

} // namespace

PrunePlan SnapshotRetention::plan(const QString& backupsDir,
                                  const RetentionPolicy& policy)
{
    PrunePlan pp;
    pp.backupsDir = QDir(backupsDir).absolutePath();

    // newest first, pre-restore-* / *relink* already excluded
    const QList<SnapshotSummary> all = SnapshotIndex::list(backupsDir);
    if (all.isEmpty() || (policy.keepLast <= 0 && policy.keepDays <= 0)) {
        for (const SnapshotSummary& s : all)
            pp.keep << s.stamp;
        return pp;
    }

    const QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < all.size(); ++i) {
        const SnapshotSummary& s = all.at(i);

        bool keep = false;
        if (i == 0)
            keep = true;                                  // never drop the newest
        if (policy.keepLast > 0 && i < policy.keepLast)
            keep = true;
        if (policy.keepDays > 0) {
            const QDateTime cap = QDateTime::fromString(s.capturedAt, Qt::ISODateWithMs);
            if (cap.isValid() && cap.daysTo(now) < policy.keepDays)
                keep = true;
        }

        if (keep) {
            pp.keep << s.stamp;
        } else {
            pp.prune << s.path;
            pp.pruneBytes += dirBytes(s.path);
        }
    }
    return pp;
}

PruneResult SnapshotRetention::apply(const PrunePlan& plan)
{
    PruneResult r;
    for (const QString& dir : plan.prune) {
        const qint64 b = dirBytes(dir);
        QStringList errs;
        if (fs::removeTree(dir, &errs)) {
            r.removed += 1;
            r.bytes += b;
        } else {
            r.errors += errs;
        }
    }
    r.ok = r.errors.isEmpty();
    return r;
}

} // namespace dm
