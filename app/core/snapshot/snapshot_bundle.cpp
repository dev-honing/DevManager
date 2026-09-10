#include "snapshot/snapshot_bundle.h"

#include "process_runner.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace dm {

namespace {

QString tarExe()
{
#ifdef Q_OS_WIN
    // Prefer the Windows bsdtar (handles C:\ paths); a stray MSYS/Git tar on
    // PATH treats "C:\..." as a remote host and fails.
    const QString sys =
        QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/tar.exe");
    if (QFileInfo::exists(sys))
        return sys;
#endif
    const QString p = QStandardPaths::findExecutable("tar");
    return p.isEmpty() ? QStringLiteral("tar") : p;
}

} // namespace

BundleResult SnapshotBundle::pack(const QString& snapshotDir, const QString& outFile)
{
    BundleResult res;
    const QFileInfo si(snapshotDir);
    if (!si.isDir() || !QFileInfo::exists(si.absoluteFilePath() + "/manifest.json")) {
        res.error = "not a snapshot dir (no manifest.json): " + snapshotDir;
        return res;
    }

    const QString parent = si.absolutePath();
    const QString name = si.fileName();
    QString out = outFile.isEmpty() ? si.absoluteFilePath() + ".tar.gz" : outFile;
    out = QDir::cleanPath(out);
    QDir().mkpath(QFileInfo(out).absolutePath());

    // -C <parent> <name>  => archive holds a single top-level "<name>/" dir
    const ProcessResult r = ProcessRunner::run(
        tarExe(),
        {"-c", "-z", "-f", QDir::toNativeSeparators(out), "-C",
         QDir::toNativeSeparators(parent), name},
        120000);
    if (!r.ok()) {
        res.error = r.started ? ("tar failed: " + r.outText() + " "
                                 + QString::fromLocal8Bit(r.err).trimmed())
                              : "tar not found";
        return res;
    }
    res.ok = true;
    res.path = QDir::toNativeSeparators(out);
    res.bytes = QFileInfo(out).size();
    return res;
}

BundleResult SnapshotBundle::unpack(const QString& bundleFile, const QString& outDir)
{
    BundleResult res;
    if (!QFileInfo::exists(bundleFile)) {
        res.error = "bundle not found: " + bundleFile;
        return res;
    }
    QDir().mkpath(outDir);

    const ProcessResult r = ProcessRunner::run(
        tarExe(),
        {"-x", "-z", "-f", QDir::toNativeSeparators(bundleFile), "-C",
         QDir::toNativeSeparators(outDir)},
        120000);
    if (!r.ok()) {
        res.error = r.started ? ("tar failed: " + QString::fromLocal8Bit(r.err).trimmed())
                              : "tar not found";
        return res;
    }

    // find the extracted snapshot dir (the one with a manifest.json)
    const auto subs = QDir(outDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& d : subs) {
        if (QFileInfo::exists(d.absoluteFilePath() + "/manifest.json")) {
            res.ok = true;
            res.path = QDir::toNativeSeparators(d.absoluteFilePath());
            return res;
        }
    }
    res.error = "no snapshot (manifest.json) found in the extracted bundle";
    return res;
}

} // namespace dm
