#include "snapshot/restore_preview.h"

#include "json_io.h"
#include "path_util.h"
#include "snapshot/snapshot_index.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace dm {

static QString remap(const QString& src, const QString& oldProfile,
                     const QString& newProfile)
{
    const QString s = QDir::fromNativeSeparators(src);
    const QString o = QDir::fromNativeSeparators(oldProfile);
    if (s.compare(o, Qt::CaseInsensitive) == 0)
        return QDir::toNativeSeparators(newProfile);
    if (s.startsWith(o + "/", Qt::CaseInsensitive))
        return QDir::toNativeSeparators(newProfile + s.mid(o.size()));
    return src;
}

RestorePreview RestorePlanner::compute(const QString& snapshotDir,
                                       const QList<ServiceState>& services)
{
    RestorePreview pv;

    const QJsonObject m = SnapshotIndex::readManifest(snapshotDir, &pv.error);
    if (m.isEmpty())
        return pv;

    pv.schemaVersion = m.value("schemaVersion").toInt();
    pv.snapshotStamp = m.value("stamp").toString(QFileInfo(snapshotDir).fileName());
    pv.sourceUserProfile = m.value("sourceUserProfile").toString();
    pv.currentUserProfile = path::homeDir();
    pv.currentUserProfile = QDir::toNativeSeparators(pv.currentUserProfile);

    const bool restoreMachine = false;   // dry-run assumes the safe default
    for (const QJsonValue& v : m.value("components").toArray()) {
        const QJsonObject c = v.toObject();
        if (!c.value("exists").toBool(true))
            continue;
        RestoreTarget t;
        t.name = c.value("name").toString();
        t.category = c.value("category").toString("core");
        t.sourcePath = c.value("source").toString();
        t.destPath = remap(t.sourcePath, pv.sourceUserProfile, pv.currentUserProfile);
        t.existsNow = QFileInfo::exists(t.destPath);
        t.included = restoreMachine || t.category != "machine";
        pv.targets << t;
    }

    // linked-skill warnings from the sibling inventory.json, if present
    const QString invPath = QDir(snapshotDir).filePath("inventory.json");
    if (QFileInfo::exists(invPath)) {
        const QJsonObject inv = json::read(invPath);
        for (const QJsonValue& sv :
             inv.value("discovery").toObject().value("skills").toArray()) {
            const QJsonObject s = sv.toObject();
            for (const QJsonValue& lv : s.value("locations").toArray()) {
                const QJsonObject l = lv.toObject();
                const QJsonObject link = l.value("link").toObject();
                if (!link.value("isLink").toBool())
                    continue;
                const QString tgt = remap(link.value("target").toString(),
                                          pv.sourceUserProfile, pv.currentUserProfile);
                if (!QFileInfo::exists(tgt))
                    pv.linkWarnings
                        << QString("%1: link target missing — %2")
                               .arg(s.value("name").toString(), tgt);
            }
        }
    }

    for (const ServiceState& s : services)
        if (s.level == ServiceState::Running
            && (s.id == "headroom" || s.id == "omniroute"))
            pv.serviceBlockers << s.name;

    pv.preRestoreBackupDir = QDir(QFileInfo(snapshotDir).absolutePath())
                                 .filePath("pre-restore-"
                                           + QDateTime::currentDateTime().toString(
                                               "yyyy-MM-ddTHHmmss"));
    pv.valid = true;
    return pv;
}

} // namespace dm
