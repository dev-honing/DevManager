#include "fs_ops.h"

#include "process_runner.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

namespace dm::fs {

QByteArray sha256Of(const QString& path)
{
    const QFileInfo fi(path);
    if (fi.isSymLink() || fi.isJunction())
        return {};

    QCryptographicHash h(QCryptographicHash::Sha256);

    if (fi.isFile()) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || !h.addData(&f))
            return {};
        return h.result();
    }
    if (!fi.isDir())
        return {};

    // work from the absolute path: a relative `path` makes QDir::relativeFilePath
    // / QDir::filePath round-trips below produce unopenable paths.
    const QString root = fi.absoluteFilePath();
    const QDir base(root);
    QStringList rels;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isSymLink() || it.fileInfo().isJunction())
            continue;   // don't hash through a reparse point
        rels << base.relativeFilePath(it.filePath());
    }
    rels.sort();

    for (const QString& rel : rels) {
        h.addData(rel.toUtf8());
        h.addData(QByteArray(1, '\0'));
        QFile f(base.filePath(rel));
        if (!f.open(QIODevice::ReadOnly) || !h.addData(&f))
            return {};
    }
    return h.result();
}

CopyStats copyTree(const QString& src, const QString& dst)
{
    CopyStats st;
    const QFileInfo fi(src);

    if (!fi.exists() && !fi.isSymLink()) {
        st.ok = false;
        st.errors << "source missing: " + QDir::toNativeSeparators(src);
        return st;
    }

    if (fi.isSymLink() || fi.isJunction()) {
        st.skippedLinks << src;
        return st;
    }

    if (fi.isFile()) {
        QDir().mkpath(QFileInfo(dst).absolutePath());
        if (QFile::exists(dst))
            QFile::remove(dst);
        if (!QFile::copy(src, dst)) {
            st.ok = false;
            st.errors << "copy failed: " + QDir::toNativeSeparators(src);
            return st;
        }
        st.bytes += fi.size();
        st.files += 1;
        return st;
    }

    if (!QDir().mkpath(dst)) {
        st.ok = false;
        st.errors << "mkdir failed: " + QDir::toNativeSeparators(dst);
        return st;
    }
    const auto entries = QDir(src).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& e : entries) {
        const CopyStats sub = copyTree(e.absoluteFilePath(), dst + "/" + e.fileName());
        st.bytes += sub.bytes;
        st.files += sub.files;
        st.skippedLinks += sub.skippedLinks;
        st.errors += sub.errors;
        st.ok = st.ok && sub.ok;
    }
    return st;
}

bool removeTree(const QString& path, QStringList* errors)
{
    const QFileInfo fi(path);
    if (!fi.exists() && !fi.isSymLink())
        return true;

    // A reparse point: unlink only. Never enter it.
    if (fi.isSymLink() || fi.isJunction()) {
        // QFile::remove works for file symlinks; QDir::rmdir for dir junctions.
        if (QFile::remove(path) || QDir().rmdir(path))
            return true;
        if (errors)
            *errors << "could not unlink: " + QDir::toNativeSeparators(path);
        return false;
    }

    if (fi.isFile()) {
        if (QFile::remove(path))
            return true;
        if (errors)
            *errors << "could not remove file: " + QDir::toNativeSeparators(path);
        return false;
    }

    bool ok = true;
    const auto entries = QDir(path).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& e : entries)
        ok = removeTree(e.absoluteFilePath(), errors) && ok;
    if (ok && !QDir().rmdir(path)) {
        if (errors)
            *errors << "could not remove dir: " + QDir::toNativeSeparators(path);
        ok = false;
    }
    return ok;
}

bool moveAside(const QString& src, const QString& destDir, const QString& name,
               QString* outMovedPath, QStringList* errors)
{
    QDir().mkpath(destDir);
    const QString dst = QDir(destDir).filePath(name);
    if (outMovedPath)
        *outMovedPath = dst;

    const bool sameVolume =
        QStorageInfo(QFileInfo(src).absolutePath()).rootPath()
        == QStorageInfo(destDir).rootPath();

    if (sameVolume && QDir().rename(src, dst))
        return true;

    // fallback: copy then remove
    const CopyStats cs = copyTree(src, dst);
    if (!cs.ok) {
        if (errors)
            *errors += cs.errors;
        return false;
    }
    return removeTree(src, errors);
}

bool recreateLink(const QString& location, const QString& target,
                  const QString& linkType, QString* error)
{
    if (!QFileInfo::exists(target)) {
        if (error)
            *error = "link target missing: " + QDir::toNativeSeparators(target);
        return false;
    }
    if (QFileInfo::exists(location) || QFileInfo(location).isSymLink()) {
        QStringList errs;
        if (!removeTree(location, &errs)) {
            if (error)
                *error = errs.join("; ");
            return false;
        }
    }
    QDir().mkpath(QFileInfo(location).absolutePath());

    const QString nloc = QDir::toNativeSeparators(location);
    const QString ntgt = QDir::toNativeSeparators(target);
    const bool junction = linkType.contains("junction", Qt::CaseInsensitive)
                          || linkType.isEmpty();

#ifdef Q_OS_WIN
    QStringList args{"/c", "mklink"};
    if (junction)
        args << "/J" << nloc << ntgt;               // no privilege needed
    else
        args << "/D" << nloc << ntgt;               // needs dev mode / admin
    const ProcessResult r = ProcessRunner::run("cmd", args, 8000);
    if (r.ok())
        return true;
    if (error)
        *error = r.outText().isEmpty() ? "mklink failed" : r.outText();
    return false;
#else
    if (QFile::link(target, location))
        return true;
    if (error)
        *error = "symlink failed";
    return false;
#endif
}

} // namespace dm::fs
