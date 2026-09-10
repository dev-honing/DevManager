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
#include "snapshot/restore_executor.h"
#include "snapshot/restore_preview.h"
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
    QCommandLineOption restoreOpt("restore", "Restore from this snapshot dir "
                                             "(prints the plan unless --apply).",
                                  "snapshot-dir");
    QCommandLineOption applyOpt("apply", "With --restore: actually apply it.");
    parser.addOption(outOpt);
    parser.addOption(rootOpt);
    parser.addOption(snapOpt);
    parser.addOption(restoreOpt);
    parser.addOption(applyOpt);
    parser.process(app);

    if (parser.isSet(rootOpt))
        QDir::setCurrent(parser.value(rootOpt));

    QTextStream err(stderr);

    // ---- restore mode --------------------------------------------------
    if (parser.isSet(restoreOpt)) {
        const QString dir = parser.value(restoreOpt);
        if (!parser.isSet(applyOpt)) {
            const dm::RestorePreview pv =
                dm::RestorePlanner::compute(dir, dm::ServiceProbe::probeAll());
            if (!pv.valid) {
                err << "error: " << pv.error << "\n";
                return 1;
            }
            err << "restore plan for " << dir << "\n"
                << "  remap: " << pv.sourceUserProfile << " -> "
                << pv.currentUserProfile << "\n"
                << "  targets: " << pv.targets.size() << "\n";
            for (const auto& t : pv.targets)
                err << "    " << (t.included ? "[x] " : "[ ] ") << t.name << "  -> "
                    << t.destPath << (t.existsNow ? "  (move aside)" : "") << "\n";
            for (const QString& w : pv.linkWarnings)
                err << "  link warn: " << w << "\n";
            for (const QString& b : pv.serviceBlockers)
                err << "  blocker: " << b << " running\n";
            err << "\n(dry run — pass --apply to execute)\n";
            return 0;
        }
        err << "APPLYING restore from " << dir << "\n";
        const dm::RestoreResult r = dm::RestoreExecutor::run(
            dir, {}, [&err](dm::RestoreState s, const QString& d) {
                err << "  " << dm::restoreStateName(s)
                    << (d.isEmpty() ? QString() : "  " + d) << "\n";
            });
        err << (r.ok ? "OK" : "FAILED") << " state=" << dm::restoreStateName(r.state)
            << " restored=" << r.restored << "\n";
        for (const QString& l : r.linkResults) err << "  link: " << l << "\n";
        for (const QString& e : r.errors) err << "  err : " << e << "\n";
        for (const QString& e : r.rollbackErrors) err << "  rb  : " << e << "\n";
        err << "  pre-restore: " << r.preRestoreDir << "\n";
        return r.ok ? 0 : 2;
    }

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
