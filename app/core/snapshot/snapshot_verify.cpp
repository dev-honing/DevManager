#include "snapshot/snapshot_verify.h"

#include "fs_ops.h"
#include "snapshot/snapshot_index.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace dm {

VerifyResult SnapshotVerify::check(const QString& snapshotDir)
{
    VerifyResult res;
    res.snapshotDir = QDir::toNativeSeparators(snapshotDir);

    const QJsonObject m = SnapshotIndex::readManifest(snapshotDir, &res.error);
    if (m.isEmpty())
        return res;

    for (const QJsonValue& v : m.value("components").toArray()) {
        const QJsonObject c = v.toObject();
        const QString recorded = c.value("sha256").toString();
        ComponentCheck cc;
        cc.name = c.value("name").toString();
        cc.backupPath = c.value("backupPath").toString();

        if (c.value("type").toString() == "link" || !c.value("copied").toBool(true)
            || recorded.isEmpty()) {
            cc.status = "no-digest";
            res.skippedCount += 1;
            res.components << cc;
            continue;
        }

        const QString path = QDir(snapshotDir).filePath(cc.backupPath);
        if (!QFileInfo::exists(path)) {
            cc.status = "missing";
            res.badCount += 1;
        } else if (QString::fromLatin1(fs::sha256Of(path).toHex()) == recorded) {
            cc.status = "ok";
            res.okCount += 1;
        } else {
            cc.status = "mismatch";
            res.badCount += 1;
        }
        res.components << cc;
    }

    res.ok = (res.badCount == 0);
    return res;
}

} // namespace dm
