#include "snapshot/snapshot_preview.h"

#include "config/scan_config.h"
#include "path_util.h"

#include <QDir>
#include <QDirIterator>

#include <algorithm>
#include <QFileInfo>

namespace dm {

QString policyName(SnapshotPolicy p)
{
    switch (p) {
    case SnapshotPolicy::Backup: return "backup";
    case SnapshotPolicy::InventoryOnly: return "inventory-only";
    case SnapshotPolicy::Regenerate: return "regenerate";
    case SnapshotPolicy::Exclude: return "exclude";
    }
    return "exclude";
}

namespace {

SnapshotPolicy parsePolicy(const QString& s)
{
    if (s == "backup") return SnapshotPolicy::Backup;
    if (s == "inventory-only") return SnapshotPolicy::InventoryOnly;
    if (s == "regenerate") return SnapshotPolicy::Regenerate;
    return SnapshotPolicy::Exclude;
}

// config rules first (root-specific before generic), then per-root default,
// then the "*" catch-all.
SnapshotPolicy classify(const ScanConfig& cfg, const QString& rootId,
                        const QString& name, QString* reason)
{
    for (const SnapshotRule& r : cfg.snapshotRules) {
        if (!r.root.isEmpty() && r.root != rootId)
            continue;
        if (!r.name.isEmpty() && r.name != name)
            continue;
        *reason = r.reason.isEmpty() ? r.policy : r.reason;
        return parsePolicy(r.policy);
    }
    if (cfg.snapshotRootDefaults.contains(rootId)) {
        *reason = "root default";
        return parsePolicy(cfg.snapshotRootDefaults.value(rootId));
    }
    *reason = "default";
    return parsePolicy(cfg.snapshotRootDefaults.value("*", "backup"));
}

// Exact recursive size. Only used for Backup-policy entries (small), so the
// dry check stays fast even with multi-GB excluded/regenerate trees around.
void sizeOf(const QString& path, qint64* bytes, int* files)
{
    QFileInfo fi(path);
    if (fi.isFile()) {
        *bytes += fi.size();
        *files += 1;
        return;
    }
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        *bytes += it.fileInfo().size();
        *files += 1;
    }
}

} // namespace

SnapshotPreview SnapshotPlanner::compute(const QList<ServiceState>& services)
{
    const ScanConfig cfg = ScanConfig::load();
    SnapshotPreview pv;
    pv.valid = true;

    for (const QString& raw : cfg.backupRoots) {
        const QString rootPath = path::expand(raw);
        const QString rootId = QFileInfo(rootPath).fileName();
        QDir rootDir(rootPath);
        if (rootPath.isEmpty() || !rootDir.exists()) {
            pv.missingRoots << (rootId.isEmpty() ? raw : rootId);
            continue;
        }

        const auto entries = rootDir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo& e : entries) {
            PlannedArtifact a;
            a.rootId = rootId;
            a.name = e.fileName();
            a.path = QDir::toNativeSeparators(e.absoluteFilePath());

            const LinkInfo link = path::probeLink(e.absoluteFilePath());
            a.isLink = link.isLink;
            a.linkType = link.linkType;
            a.linkTarget = link.target;

            a.policy = classify(cfg, rootId, a.name, &a.reason);
            if (a.isLink && a.policy == SnapshotPolicy::Backup)
                a.reason = "link — recreated, not copied";

            switch (a.policy) {
            case SnapshotPolicy::Backup:
                if (!a.isLink)                     // links carry no bytes
                    sizeOf(e.absoluteFilePath(), &a.sizeBytes, &a.fileCount);
                pv.backupBytes += a.sizeBytes;
                pv.backupFiles += a.fileCount;
                break;
            case SnapshotPolicy::InventoryOnly:
                pv.inventoryOnlyCount += 1;
                break;
            case SnapshotPolicy::Regenerate:
                pv.regenerateCount += 1;
                // don't recurse multi-GB trees; a top-level child count is enough
                a.fileCount = e.isDir()
                    ? QDir(e.absoluteFilePath()).entryList(
                          QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size()
                    : 1;
                break;
            case SnapshotPolicy::Exclude:
                pv.excludeCount += 1;
                break;
            }
            pv.artifacts << a;
        }
    }

    // most relevant first: backup > regenerate > inventory-only > exclude
    auto rank = [](SnapshotPolicy p) {
        switch (p) {
        case SnapshotPolicy::Backup: return 0;
        case SnapshotPolicy::Regenerate: return 1;
        case SnapshotPolicy::InventoryOnly: return 2;
        case SnapshotPolicy::Exclude: return 3;
        }
        return 3;
    };
    std::stable_sort(pv.artifacts.begin(), pv.artifacts.end(),
                     [&](const PlannedArtifact& a, const PlannedArtifact& b) {
                         if (rank(a.policy) != rank(b.policy))
                             return rank(a.policy) < rank(b.policy);
                         return a.rootId + a.name < b.rootId + b.name;
                     });

    QStringList blockerIds;
    for (const ServiceSpec& s : cfg.services)
        if (s.snapshotBlocker)
            blockerIds << s.id;
    for (const ServiceState& s : services)
        if (s.level == ServiceState::Running && blockerIds.contains(s.id))
            pv.serviceBlockers << s.name;

    return pv;
}

} // namespace dm
