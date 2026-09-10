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

struct Rule {
    QString rootId;     // "" = any
    QString name;       // "" = any top-level entry under rootId
    SnapshotPolicy policy;
    QString reason;
};

// First match wins. Default (no match) is per-root, see rootDefault().
// ponytail: built-in default table; move to config alongside backupRoots
//           if users need to retune it per machine.
const QList<Rule>& rules()
{
    static const QList<Rule> t = {
        // .claude
        {".claude", "settings.json", SnapshotPolicy::Backup, "core config"},
        {".claude", "CLAUDE.md", SnapshotPolicy::Backup, "user rules"},
        {".claude", ".credentials.json", SnapshotPolicy::Exclude, "secret — re-login"},
        {".claude", "skills", SnapshotPolicy::Backup, "user skills"},
        {".claude", "plugins", SnapshotPolicy::Regenerate, "re-install from marketplace"},
        {".claude", "projects", SnapshotPolicy::InventoryOnly, "session transcripts"},
        {".claude", "history.jsonl", SnapshotPolicy::InventoryOnly, "prompt history"},
        {".claude", "cache", SnapshotPolicy::Exclude, "cache"},
        {".claude", "file-history", SnapshotPolicy::Exclude, "editor undo"},
        {".claude", "shell-snapshots", SnapshotPolicy::Exclude, "runtime"},
        // .codex
        {".codex", "config.toml", SnapshotPolicy::Backup, "core config"},
        {".codex", "AGENTS.md", SnapshotPolicy::Backup, "user rules"},
        {".codex", "hooks.json", SnapshotPolicy::Backup, "config"},
        {".codex", "auth.json", SnapshotPolicy::Exclude, "secret — re-login"},
        {".codex", "skills", SnapshotPolicy::Backup, "user skills"},
        {".codex", "plugins", SnapshotPolicy::Regenerate, "re-install"},
        {".codex", ".tmp", SnapshotPolicy::Exclude, "scratch"},
        {".codex", ".sandbox-bin", SnapshotPolicy::Exclude, "runtime"},
        {".codex", "sessions", SnapshotPolicy::InventoryOnly, "session data"},
        {".codex", "cache", SnapshotPolicy::Exclude, "cache"},
        // .headroom
        {".headroom", "deploy", SnapshotPolicy::Backup, "deployment definition"},
        {".headroom", "config", SnapshotPolicy::Backup, "config"},
        {".headroom", "logs", SnapshotPolicy::Exclude, "logs"},
        // .omniroute
        {".omniroute", ".env", SnapshotPolicy::Exclude, "secret — re-login"},
        {".omniroute", "storage.sqlite", SnapshotPolicy::Backup, "routing state"},
        {".omniroute", "call_logs", SnapshotPolicy::Exclude, "logs"},
        {".omniroute", "db_backups", SnapshotPolicy::Exclude, "logs"},
        {".omniroute", "logs", SnapshotPolicy::Exclude, "logs"},
    };
    return t;
}

SnapshotPolicy rootDefault(const QString& rootId, QString* reason)
{
    if (rootId == ".agents" || rootId == ".gemini") {
        *reason = "config";
        return SnapshotPolicy::Backup;
    }
    if (rootId == ".claude" || rootId == ".codex") {
        *reason = "runtime";
        return SnapshotPolicy::Exclude;
    }
    *reason = "default";
    return SnapshotPolicy::Backup;
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
            a.linkTarget = link.target;

            bool matched = false;
            for (const Rule& r : rules()) {
                if ((r.rootId.isEmpty() || r.rootId == rootId)
                    && (r.name.isEmpty() || r.name == a.name)) {
                    a.policy = r.policy;
                    a.reason = r.reason;
                    matched = true;
                    break;
                }
            }
            if (!matched)
                a.policy = rootDefault(rootId, &a.reason);
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

    for (const ServiceState& s : services)
        if (s.level == ServiceState::Running
            && (s.id == "headroom" || s.id == "omniroute"))
            pv.serviceBlockers << s.name;

    return pv;
}

} // namespace dm
