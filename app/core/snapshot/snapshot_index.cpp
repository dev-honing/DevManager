#include "snapshot/snapshot_index.h"

#include "json_io.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace dm {

QString SnapshotIndex::findBackupsDir(const QString& startDir)
{
    QDir d(startDir);
    for (int i = 0; i < 6; ++i) {
        if (d.exists("backups"))
            return QDir(d.filePath("backups")).absolutePath();
        if (!d.cdUp())
            break;
    }
    return {};
}

QList<SnapshotSummary> SnapshotIndex::list(const QString& backupsDir)
{
    QList<SnapshotSummary> out;
    QDir dir(backupsDir);
    if (!dir.exists())
        return out;

    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                           QDir::Name | QDir::Reversed);
    for (const QFileInfo& e : entries) {
        const QString name = e.fileName();
        if (name.startsWith("pre-restore") || name.contains("relink"))
            continue;
        const QString manifest = e.filePath() + "/manifest.json";
        if (!QFileInfo::exists(manifest))
            continue;

        const QJsonObject m = json::read(manifest);
        if (m.isEmpty())
            continue;

        SnapshotSummary s;
        s.stamp = m.value("stamp").toString(name);
        s.path = e.absoluteFilePath();
        s.capturedAt = m.value("capturedAt").toString();
        s.componentCount = m.value("components").toArray().size();
        s.consistent = m.value("consistentSnapshot").toBool(false);
        s.producer = m.value("snapshotVersion").toString();
        out << s;
    }
    return out;
}

} // namespace dm
