#include "snapshot/restore_executor.h"

#include "fs_ops.h"
#include "json_io.h"
#include "path_util.h"
#include "snapshot/snapshot_index.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace dm {

QString restoreStateName(RestoreState s)
{
    switch (s) {
    case RestoreState::Planned: return "planned";
    case RestoreState::Preserving: return "preserving current config";
    case RestoreState::Restoring: return "restoring snapshot";
    case RestoreState::Relinking: return "recreating links";
    case RestoreState::Verifying: return "verifying";
    case RestoreState::Done: return "done";
    case RestoreState::RollingBack: return "rolling back";
    case RestoreState::RolledBack: return "rolled back";
    case RestoreState::Failed: return "failed";
    }
    return "?";
}

namespace {

QString remap(const QString& src, const QString& oldP, const QString& newP)
{
    const QString s = QDir::fromNativeSeparators(src);
    const QString o = QDir::fromNativeSeparators(oldP);
    const QString n = QDir::fromNativeSeparators(newP);
    if (s.compare(o, Qt::CaseInsensitive) == 0)
        return QDir::toNativeSeparators(n);
    if (s.startsWith(o + "/", Qt::CaseInsensitive))
        return QDir::toNativeSeparators(n + s.mid(o.size()));
    return src;
}

// undo: remove restored targets (reverse), then move preserved items back
void rollback(RestoreResult& res)
{
    res.state = RestoreState::RollingBack;
    for (int i = res.steps.size() - 1; i >= 0; --i) {
        const RestoreStep& st = res.steps.at(i);
        if (st.restored)
            fs::removeTree(st.destPath, &res.rollbackErrors);
    }
    for (int i = res.steps.size() - 1; i >= 0; --i) {
        const RestoreStep& st = res.steps.at(i);
        if (st.preservedPath.isEmpty())
            continue;
        if (QFileInfo::exists(st.destPath))
            fs::removeTree(st.destPath, &res.rollbackErrors);
        QString back;
        fs::moveAside(st.preservedPath, QFileInfo(st.destPath).absolutePath(),
                      QFileInfo(st.destPath).fileName(), &back, &res.rollbackErrors);
    }
    res.state = res.rollbackErrors.isEmpty() ? RestoreState::RolledBack
                                             : RestoreState::Failed;
    res.ok = false;
}

} // namespace

RestoreResult RestoreExecutor::run(const QString& snapshotDir,
                                   const RestoreOptions& opts, Progress progress)
{
    RestoreResult res;
    res.snapshotDir = QDir::toNativeSeparators(snapshotDir);
    auto tick = [&](RestoreState s, const QString& d = {}) {
        res.state = s;
        if (progress)
            progress(s, d);
    };

    // ---- read manifest ------------------------------------------------------
    QString manifestErr;
    const QJsonObject m = SnapshotIndex::readManifest(snapshotDir, &manifestErr);
    if (m.isEmpty()) {
        res.errors << manifestErr;
        res.state = RestoreState::Failed;
        return res;
    }
    const QString sourceUP = m.value("sourceUserProfile").toString();
    const QString currentUP = QDir::toNativeSeparators(path::homeDir());
    if (sourceUP.isEmpty()) {
        res.errors << "manifest has no sourceUserProfile";
        res.state = RestoreState::Failed;
        return res;
    }

    struct Item {
        QString name, category, backupPath, destPath, type;
    };
    QList<Item> items;
    for (const QJsonValue& v : m.value("components").toArray()) {
        const QJsonObject c = v.toObject();
        if (c.value("type").toString() == "link")
            continue;   // links are rebuilt in the Relinking phase
        if (!c.value("copied").toBool(true))
            continue;
        Item it;
        it.name = c.value("name").toString();
        it.category = c.value("category").toString("core");
        it.backupPath = c.value("backupPath").toString();
        it.type = c.value("type").toString("directory");
        if (it.category == "machine" && !opts.includeMachine)
            continue;
        it.destPath = remap(c.value("source").toString(), sourceUP, currentUP);
        if (it.backupPath.isEmpty() || it.destPath.isEmpty())
            continue;
        items << it;
    }
    if (items.isEmpty()) {
        res.errors << "nothing to restore (no copied components)";
        res.state = RestoreState::Failed;
        return res;
    }

    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-ddTHHmmss");
    res.preRestoreDir = QDir::toNativeSeparators(
        QDir(QFileInfo(snapshotDir).absolutePath()).filePath("pre-restore-" + stamp));
    QDir().mkpath(res.preRestoreDir);

    // ---- preserve ---------------------------------------------------------
    tick(RestoreState::Preserving);
    int idx = 0;
    for (const Item& it : items) {
        RestoreStep st;
        st.name = it.name;
        st.destPath = it.destPath;
        if (QFileInfo::exists(it.destPath) || QFileInfo(it.destPath).isSymLink()) {
            QString moved;
            const QString tag = QString("%1_%2").arg(idx, 3, 10, QChar('0'))
                                    .arg(QFileInfo(it.destPath).fileName());
            if (!fs::moveAside(it.destPath, res.preRestoreDir, tag, &moved,
                               &res.errors)) {
                res.errors << "preserve failed for " + it.name;
                rollback(res);
                return res;
            }
            st.preservedPath = moved;
        }
        res.steps << st;
        ++idx;
    }

    // ---- restore --------------------------------------------------------
    tick(RestoreState::Restoring);
    for (int i = 0; i < items.size(); ++i) {
        const Item& it = items.at(i);
        tick(RestoreState::Restoring, it.name);
        const QString from = QDir(snapshotDir).filePath(it.backupPath);
        const fs::CopyStats cs = fs::copyTree(from, it.destPath);
        if (!cs.ok) {
            res.errors += cs.errors;
            rollback(res);
            return res;
        }
        res.steps[i].restored = true;
        res.restored += 1;
    }

    // ---- relink (scoped to the roots we just restored) ---------------
    // Only rebuild a link whose location sits inside something this run
    // actually restored; links elsewhere in the inventory are left alone.
    if (opts.recreateLinks) {
        tick(RestoreState::Relinking);
        QStringList restoredRoots;
        for (const RestoreStep& st : res.steps)
            if (st.restored)
                restoredRoots << QDir::fromNativeSeparators(st.destPath);
        auto underRestored = [&](const QString& p) {
            const QString s = QDir::fromNativeSeparators(p);
            for (const QString& r : restoredRoots)
                if (s.compare(r, Qt::CaseInsensitive) == 0
                    || s.startsWith(r + "/", Qt::CaseInsensitive))
                    return true;
            return false;
        };
        const QJsonObject inv =
            json::read(QDir(snapshotDir).filePath("inventory.json"));
        for (const QJsonValue& sv :
             inv.value("discovery").toObject().value("skills").toArray()) {
            const QJsonObject s = sv.toObject();
            const QString skill = s.value("name").toString();
            for (const QJsonValue& lv : s.value("locations").toArray()) {
                const QJsonObject l = lv.toObject();
                const QJsonObject link = l.value("link").toObject();
                if (!link.value("isLink").toBool())
                    continue;
                const QString loc =
                    remap(l.value("path").toString(), sourceUP, currentUP);
                if (!underRestored(loc)) {
                    res.linkResults << skill + " @ " + QFileInfo(loc).path()
                                           + ": skipped (outside restored scope)";
                    continue;
                }
                const QString tgt =
                    remap(link.value("target").toString(), sourceUP, currentUP);
                QString e;
                if (fs::recreateLink(loc, tgt, link.value("linkType").toString(), &e))
                    res.linkResults << skill + " @ " + QFileInfo(loc).path()
                                           + ": recreated";
                else
                    res.linkResults << skill + " @ " + QFileInfo(loc).path()
                                           + ": " + e;
            }
        }
    }

    // ---- verify ------------------------------------------------------
    tick(RestoreState::Verifying);
    for (const RestoreStep& st : res.steps) {
        if (st.restored && !QFileInfo::exists(st.destPath)) {
            res.errors << "verify: missing after restore -> " + st.destPath;
            rollback(res);
            return res;
        }
    }

    res.state = RestoreState::Done;
    res.ok = true;
    tick(RestoreState::Done);
    return res;
}

} // namespace dm
