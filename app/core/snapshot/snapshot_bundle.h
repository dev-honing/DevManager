#pragma once
//
// Pack a snapshot directory into a single portable archive (and back), so it
// can be carried to another machine and restored there. Uses the system `tar`
// (bsdtar on Windows 10+, GNU tar elsewhere) -- one file, gzip-compressed.
//
// Secrets are never inside a snapshot (the policy table excludes credential
// files), so the bundle carries no secret payload and needs no encryption.
//
#include <QString>

namespace dm {

struct BundleResult {
    bool ok = false;
    QString error;
    QString path;      // pack: the archive; unpack: the extracted snapshot dir
    qint64 bytes = 0;  // pack only
};

class SnapshotBundle {
public:
    // `outFile` "" => "<snapshotDir>.tar.gz" beside the source.
    static BundleResult pack(const QString& snapshotDir, const QString& outFile = {});

    // Extracts into `outDir` and returns the snapshot dir inside it (the one
    // holding manifest.json). `outDir` is created if missing.
    static BundleResult unpack(const QString& bundleFile, const QString& outDir);
};

} // namespace dm
