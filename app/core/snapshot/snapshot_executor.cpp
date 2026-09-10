#include "snapshot/snapshot_executor.h"

#include "fs_ops.h"
#include "json_io.h"
#include "path_util.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcessEnvironment>

namespace dm {

SnapshotResult SnapshotExecutor::run(const SnapshotPreview& preview,
                                     const QString& destBaseDir,
                                     const QJsonObject& inventoryJson,
                                     Progress progress)
{
    SnapshotResult res;
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-ddTHHmmss");
    const QString snapDir = QDir(destBaseDir).filePath(stamp);

    if (!QDir().mkpath(snapDir)) {
        res.errors << "could not create " + QDir::toNativeSeparators(snapDir);
        return res;
    }
    res.snapshotDir = QDir::toNativeSeparators(snapDir);

    QList<PlannedArtifact> backup;
    for (const PlannedArtifact& a : preview.artifacts)
        if (a.policy == SnapshotPolicy::Backup)
            backup << a;

    QJsonArray components;
    const int total = backup.size();
    int done = 0;

    for (const PlannedArtifact& a : backup) {
        if (progress)
            progress(done, total, a.rootId + "/" + a.name);

        const QString rel = a.rootId + "/" + a.name;
        QJsonObject c{
            {"name", a.rootId + "-" + a.name},
            {"category", "core"},
            {"source", a.path},
            {"backupPath", rel},
            {"policy", "backup"},
            {"reason", a.reason},
            {"exists", true},
        };

        if (a.isLink) {
            c.insert("type", "link");
            c.insert("isLink", true);
            c.insert("linkType", a.linkType);
            c.insert("linkTarget", a.linkTarget);
            c.insert("copied", false);
            res.linkNotes << rel + "  ->  " + a.linkTarget;
        } else {
            const fs::CopyStats cs = fs::copyTree(a.path, snapDir + "/" + rel);
            c.insert("type", QFileInfo(a.path).isDir() ? "directory" : "file");
            c.insert("sizeBytes", static_cast<double>(cs.bytes));
            c.insert("fileCount", cs.files);
            for (const QString& s : cs.skippedLinks)
                res.linkNotes << "skipped link inside " + rel + ": "
                                     + QDir::toNativeSeparators(s);

            // A locked file (common in a live snapshot) is a partial copy, not
            // a failure. Only a component where nothing at all copied fails.
            const bool anything = cs.files > 0;
            if (cs.ok || anything) {
                res.copied += 1;
                res.bytes += cs.bytes;
                c.insert("copied", true);
                const QByteArray dg = fs::sha256Of(snapDir + "/" + rel);
                if (!dg.isEmpty())
                    c.insert("sha256", QString::fromLatin1(dg.toHex()));
                if (!cs.ok) {
                    c.insert("partial", true);
                    res.linkNotes << QString("%1: %2 file(s) locked/skipped")
                                         .arg(rel)
                                         .arg(cs.errors.size());
                }
            } else {
                res.failed += 1;
                res.errors += cs.errors;
                c.insert("copied", false);
                c.insert("error", cs.errors.join("; "));
            }
        }
        components.append(c);
        ++done;
    }

    if (progress)
        progress(total, total, "writing manifest");

    QJsonObject manifest{
        {"schemaVersion", 5},
        {"snapshotVersion", "5.0"},
        {"producedBy", "DevManager/0.1 (C++/Qt6)"},
        {"stamp", stamp},
        {"capturedAt", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
        {"sourceMachine", env.value("COMPUTERNAME")},
        {"sourceUserProfile", QDir::toNativeSeparators(path::homeDir())},
        {"sourceRoot", QDir::toNativeSeparators(QFileInfo(destBaseDir).absolutePath())},
        {"inventoryFile", inventoryJson.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                  : QJsonValue("inventory.json")},
        {"consistentSnapshot", preview.serviceBlockers.isEmpty()},
        {"serviceBlockers", QJsonArray::fromStringList(preview.serviceBlockers)},
        {"components", components},
        {"skipped", QJsonObject{
                        {"inventory-only", preview.inventoryOnlyCount},
                        {"regenerate", preview.regenerateCount},
                        {"exclude", preview.excludeCount},
                    }},
        {"warnings", QJsonArray{
                         "Backup data may contain OAuth tokens and API keys — keep it private.",
                         "Reparse points are recorded as link metadata, not copied.",
                         "regenerate items (plugins, node_modules) must be re-installed on restore.",
                     }},
    };

    QString err;
    if (!json::write(QDir(snapDir).filePath("manifest.json"), manifest, &err)) {
        res.errors << "manifest write failed: " + err;
        return res;
    }
    if (!inventoryJson.isEmpty())
        json::write(QDir(snapDir).filePath("inventory.json"), inventoryJson);

    res.ok = (res.failed == 0);
    return res;
}

} // namespace dm
