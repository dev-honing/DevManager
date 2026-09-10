#include "snapshot/snapshot_executor.h"

#include "json_io.h"
#include "path_util.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcessEnvironment>

namespace dm {

// Recursive copy that NEVER follows a reparse point (junction/symlink):
// those are recorded and skipped, so a snapshot can't inflate a link into a
// full copy (the bug that corrupted gpt-image under the PowerShell restore).
static bool copyEntry(const QString& src, const QString& dst, qint64* bytes,
                      int* files, QStringList* errors, QStringList* skippedLinks)
{
    const QFileInfo fi(src);

    if (fi.isSymLink() || fi.isJunction()) {
        *skippedLinks << src;
        return true;
    }

    if (fi.isFile()) {
        QDir().mkpath(QFileInfo(dst).absolutePath());
        if (QFile::exists(dst))
            QFile::remove(dst);
        if (!QFile::copy(src, dst)) {
            *errors << "copy failed: " + QDir::toNativeSeparators(src);
            return false;
        }
        *bytes += fi.size();
        *files += 1;
        return true;
    }

    QDir().mkpath(dst);
    bool ok = true;
    const auto entries = QDir(src).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& e : entries) {
        ok &= copyEntry(e.absoluteFilePath(), dst + "/" + e.fileName(), bytes,
                        files, errors, skippedLinks);
    }
    return ok;
}

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
            qint64 bytes = 0;
            int files = 0;
            QStringList errs, skipped;
            const bool ok = copyEntry(a.path, snapDir + "/" + rel, &bytes, &files,
                                      &errs, &skipped);
            c.insert("type", QFileInfo(a.path).isDir() ? "directory" : "file");
            c.insert("copied", ok);
            c.insert("sizeBytes", static_cast<double>(bytes));
            c.insert("fileCount", files);
            for (const QString& s : skipped)
                res.linkNotes << "skipped link inside " + rel + ": "
                                     + QDir::toNativeSeparators(s);
            if (ok) {
                res.copied += 1;
                res.bytes += bytes;
            } else {
                res.failed += 1;
                res.errors += errs;
                c.insert("error", errs.join("; "));
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
