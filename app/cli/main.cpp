//
// devmanager-scan  --  read-only environment scan + optional snapshot.
//
// Default: writes an inventory JSON, touches nothing else.
// --snapshot: also writes a real snapshot under <root>/backups/<stamp>/
//             (reads the configured dirs, writes only there).
//
#include "json_io.h"
#include "scan/env_scanner.h"
#include "service/service_probe.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_index.h"
#include "snapshot/snapshot_preview.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("devmanager-scan");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Read-only scan of the local AI dev environment -> inventory JSON.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption outOpt({"o", "out"}, "Output JSON path.", "path",
                             "inventory.cpp.json");
    QCommandLineOption rootOpt("root", "Project root recorded in the inventory.",
                              "path");
    QCommandLineOption snapOpt("snapshot",
                               "Also create a snapshot under <root>/backups/.");
    parser.addOption(outOpt);
    parser.addOption(rootOpt);
    parser.addOption(snapOpt);
    parser.process(app);

    if (parser.isSet(rootOpt))
        QDir::setCurrent(parser.value(rootOpt));

    QTextStream err(stderr);
    err << "scanning environment (read-only)...\n";

    const dm::EnvironmentInventory inv = dm::EnvironmentScanner::scan();
    const QJsonObject invJson = inv.toJson();

    QString writeErr;
    const QString out = parser.value(outOpt);
    if (!dm::json::write(out, invJson, &writeErr)) {
        err << "error: could not write " << out << ": " << writeErr << "\n";
        return 1;
    }
    err << "wrote " << out << "\n"
        << "  tools/skills/plugins/packages/env : " << inv.tools.size() << "/"
        << inv.skills.size() << "/" << inv.plugins.size() << "/"
        << inv.globalPackages.size() << "/" << inv.env.size() << "\n";

    if (!parser.isSet(snapOpt))
        return 0;

    QString backups = dm::SnapshotIndex::findBackupsDir(QDir::currentPath());
    if (backups.isEmpty())
        backups = QDir(QDir::currentPath()).filePath("backups");

    err << "\ncomputing snapshot plan...\n";
    const dm::SnapshotPreview pv =
        dm::SnapshotPlanner::compute(dm::ServiceProbe::probeAll());
    if (!pv.serviceBlockers.isEmpty())
        err << "  live: " << pv.serviceBlockers.join(", ")
            << " running -> non-consistent snapshot\n";
    err << "  backup " << pv.backupFiles << " files ("
        << (pv.backupBytes / 1024) << " KB), exclude " << pv.excludeCount
        << ", regenerate " << pv.regenerateCount << ", inventory-only "
        << pv.inventoryOnlyCount << "\n";

    const dm::SnapshotResult r = dm::SnapshotExecutor::run(
        pv, backups, invJson, [&err](int done, int total, const QString& label) {
            err << "  [" << done << "/" << total << "] " << label << "\n";
        });

    err << (r.ok ? "OK   " : "FAIL ") << r.snapshotDir << "\n"
        << "  copied " << r.copied << ", failed " << r.failed << ", "
        << (r.bytes / 1024) << " KB\n";
    for (const QString& n : r.linkNotes)
        err << "  link: " << n << "\n";
    for (const QString& e : r.errors)
        err << "  err : " << e << "\n";
    return r.ok ? 0 : 2;
}
