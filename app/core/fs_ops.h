#pragma once
//
// Filesystem primitives with one hard rule: NEVER recurse through a reparse
// point (junction / symlink). Copying one would inflate a link into a full
// tree; deleting one would wipe the link target. Both are the bug that
// corrupted gpt-image under the PowerShell restore.
//
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace dm::fs {

// SHA-256 of a file, or of a directory tree (each file's relative path + its
// bytes, folded in sorted-path order for a stable result). Reparse points are
// skipped, never followed. Returns an empty QByteArray on any read error.
QByteArray sha256Of(const QString& path);

struct CopyStats {
    bool ok = true;
    qint64 bytes = 0;
    int files = 0;
    QStringList skippedLinks;   // reparse points encountered, recorded not copied
    QStringList errors;
};

// Recursive copy. Reparse points under `src` are recorded in skippedLinks and
// NOT followed. Existing files at the destination are overwritten.
CopyStats copyTree(const QString& src, const QString& dst);

// Recursive delete. A reparse point is unlinked (never entered). Real dirs and
// files are removed. Returns false and fills `errors` on any failure.
bool removeTree(const QString& path, QStringList* errors = nullptr);

// Move `src` to `<destDir>/<name>`. Same-volume => rename (fast, atomic-ish);
// cross-volume => copyTree + removeTree. `outMovedPath` gets the final path.
bool moveAside(const QString& src, const QString& destDir, const QString& name,
               QString* outMovedPath, QStringList* errors = nullptr);

// Recreate a junction (linkType contains "Junction") or symbolic link at
// `location` pointing at `target`. Returns false if `target` does not exist
// (a broken link is never created) or the OS call fails.
bool recreateLink(const QString& location, const QString& target,
                  const QString& linkType, QString* error = nullptr);

} // namespace dm::fs
