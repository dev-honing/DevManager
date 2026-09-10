//
// devmanager-scan  --  read-only environment scan + optional snapshot.
//
// Default: writes an inventory JSON, touches nothing else.
// --snapshot: also writes a real snapshot under <root>/backups/<stamp>/
//             (reads the configured dirs, writes only there).
//
#include "bootstrap.h"
#include "health_check.h"
#include "json_io.h"
#include "process_runner.h"
#include "project/project_env.h"
#include "scan/env_scanner.h"
#include "service/service_lifecycle.h"
#include "service/service_probe.h"
#include "snapshot/restore_executor.h"
#include "snapshot/restore_preview.h"
#include "snapshot/snapshot_bundle.h"
#include "snapshot/snapshot_diff.h"
#include "snapshot/snapshot_executor.h"
#include "snapshot/snapshot_index.h"
#include "snapshot/snapshot_preview.h"
#include "snapshot/snapshot_retention.h"
#include "snapshot/snapshot_verify.h"

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
    QCommandLineOption verifyOpt("verify", "Re-hash a snapshot dir and check it "
                                           "against its manifest.",
                                 "snapshot-dir");
    QCommandLineOption diffOpt("diff", "Compare this snapshot dir (A)...", "dir-a");
    QCommandLineOption diffToOpt("diff-to", "...against this one (B).", "dir-b");
    QCommandLineOption bundleOpt("bundle", "Pack a snapshot dir into one archive.",
                                 "snapshot-dir");
    QCommandLineOption bundleOutOpt("bundle-out", "Archive path (default: <dir>.tar.gz).",
                                    "file");
    QCommandLineOption unbundleOpt("unbundle", "Extract a snapshot archive.", "file");
    QCommandLineOption unbundleOutOpt("unbundle-out", "Where to extract (default: cwd).",
                                     "dir");
    QCommandLineOption healthOpt("health", "Check configured tools / backup roots / "
                                           "services are ready (read-only).");
    QCommandLineOption profileOpt("profile", "With --health: only check this host "
                                             "profile's tools (e.g. msvc-qt6).", "name");
    QCommandLineOption bootstrapOpt("bootstrap", "Print install commands for the "
                                                 "configured tools that are missing "
                                                 "(does not run them).");
    QCommandLineOption serviceOpt("service", "Service id to control (with --service-op).",
                                  "id");
    QCommandLineOption serviceOpOpt("service-op", "status | stop | start | restart.",
                                    "op", "status");
    QCommandLineOption projInitOpt("project-init", "Generate .devmanager/docker-compose.yml "
                                                   "+ .devcontainer/ for a project.", "name");
    QCommandLineOption projTypeOpt("project-type", "cpp | nextjs | ... (project-types.json).",
                                   "type", "cpp");
    QCommandLineOption projUpOpt("project-up", "docker compose up -d for the generated project.");
    QCommandLineOption projDownOpt("project-down", "docker compose down for it.");
    QCommandLineOption pruneOpt("prune", "Show which snapshots retention would drop "
                                         "(add --apply to delete them).");
    QCommandLineOption keepLastOpt("keep-last", "prune: keep the N newest snapshots.",
                                   "N", "0");
    QCommandLineOption keepDaysOpt("keep-days", "prune: keep snapshots newer than D days.",
                                   "D", "0");
    parser.addOption(outOpt);
    parser.addOption(rootOpt);
    parser.addOption(snapOpt);
    parser.addOption(restoreOpt);
    parser.addOption(applyOpt);
    parser.addOption(verifyOpt);
    parser.addOption(diffOpt);
    parser.addOption(diffToOpt);
    parser.addOption(bundleOpt);
    parser.addOption(bundleOutOpt);
    parser.addOption(unbundleOpt);
    parser.addOption(unbundleOutOpt);
    parser.addOption(healthOpt);
    parser.addOption(profileOpt);
    parser.addOption(bootstrapOpt);
    parser.addOption(serviceOpt);
    parser.addOption(serviceOpOpt);
    parser.addOption(projInitOpt);
    parser.addOption(projTypeOpt);
    parser.addOption(projUpOpt);
    parser.addOption(projDownOpt);
    parser.addOption(pruneOpt);
    parser.addOption(keepLastOpt);
    parser.addOption(keepDaysOpt);
    parser.process(app);

    if (parser.isSet(rootOpt))
        QDir::setCurrent(parser.value(rootOpt));

    QTextStream err(stderr);

    // ---- health mode -----------------------------------------------
    if (parser.isSet(healthOpt)) {
        const dm::HealthReport h = dm::HealthCheck::run(parser.value(profileOpt));
        for (const auto& it : h.items)
            err << "  " << it.status << "  [" << it.group << "] " << it.name
                << (it.detail.isEmpty() ? QString() : "  -- " + it.detail) << "\n";
        err << (h.ok ? "READY " : "NOT READY ") << h.okCount << " ok, "
            << h.warnCount << " warn, " << h.failCount << " fail\n";
        return h.ok ? 0 : 2;
    }

    // ---- service lifecycle -------------------------------------------
    if (parser.isSet(serviceOpt)) {
        const QString id = parser.value(serviceOpt);
        const QString opStr = parser.value(serviceOpOpt).toLower();
        dm::LifecycleOp op = dm::LifecycleOp::Status;
        if (opStr == "stop") op = dm::LifecycleOp::Stop;
        else if (opStr == "start") op = dm::LifecycleOp::Start;
        else if (opStr == "restart") op = dm::LifecycleOp::Restart;
        else if (opStr != "status") {
            err << "error: --service-op must be status|stop|start|restart\n";
            return 1;
        }
        const dm::LifecycleResult r = dm::ServiceLifecycle::run(id, op);
        err << (r.ok ? "OK  " : "FAIL ") << id << " " << dm::lifecycleOpName(op) << "\n";
        if (!r.output.isEmpty())
            err << r.output << "\n";
        if (!r.error.isEmpty())
            err << "  err: " << r.error << "\n";
        return r.ok ? 0 : 2;
    }

    // ---- project container setup (P11.2/11.3) ------------------------
    if (parser.isSet(projInitOpt) || parser.isSet(projUpOpt) || parser.isSet(projDownOpt)) {
        const QString cwd = QDir::currentPath();
        const QString composePath = cwd + "/.devmanager/docker-compose.yml";

        if (parser.isSet(projInitOpt)) {
            const dm::ProjectSpec s = dm::ProjectEnv::resolve(
                parser.value(projInitOpt), parser.value(projTypeOpt));
            if (!s.valid) {
                err << "error: " << s.error << "\n";
                return 1;
            }
            if (!s.docker) {
                err << s.name << ": host profile '" << s.hostProfile
                    << "' -- no container, use --health --profile " << s.hostProfile << "\n";
                return 0;
            }
            QString e;
            if (!dm::json::writeText(composePath, dm::ProjectEnv::composeYaml(s), &e)
                || !dm::json::writeText(cwd + "/.devcontainer/devcontainer.json",
                                        dm::ProjectEnv::devcontainerJson(s), &e)) {
                err << "error: " << e << "\n";
                return 1;
            }
            err << "wrote .devmanager/docker-compose.yml + .devcontainer/devcontainer.json\n"
                << "  image " << s.image << ", volumes " << s.volumes.join(", ") << "\n"
                << "next:  devmanager-scan --project-up\n";
            return 0;
        }

        if (!QFileInfo::exists(composePath)) {
            err << "error: no .devmanager/docker-compose.yml -- run --project-init first\n";
            return 1;
        }
        const QStringList args = parser.isSet(projUpOpt)
                                     ? QStringList{"compose", "-f", composePath, "up", "-d"}
                                     : QStringList{"compose", "-f", composePath, "down"};
        const dm::ProcessResult r = dm::ProcessRunner::run("docker", args, 120000);
        err << QString::fromLocal8Bit(r.out) << QString::fromLocal8Bit(r.err);
        return r.ok() ? 0 : 2;
    }

    // ---- bootstrap: print install commands for missing tools ----------
    if (parser.isSet(bootstrapOpt)) {
        const dm::BootstrapPlan bp = dm::Bootstrap::plan();
        QTextStream out(stdout);
        out << "# install commands for missing tools -- review before running\n";
        if (bp.missing == 0)
            out << "# (nothing missing)\n";
        for (const dm::BootstrapStep& s : bp.steps) {
            if (s.haveHint)
                out << s.command << "\n";
            else
                out << "# " << s.tool << ": no install hint in config -- install manually\n";
        }
        err << bp.missing << " missing, " << bp.withHint << " with an install hint\n";
        return 0;
    }

    // ---- prune (retention) ---------------------------------------------
    if (parser.isSet(pruneOpt)) {
        dm::RetentionPolicy pol;
        pol.keepLast = parser.value(keepLastOpt).toInt();
        pol.keepDays = parser.value(keepDaysOpt).toInt();
        if (pol.keepLast <= 0 && pol.keepDays <= 0) {
            err << "error: --prune needs --keep-last N and/or --keep-days D\n";
            return 1;
        }
        QString backups = dm::SnapshotIndex::findBackupsDir(QDir::currentPath());
        if (backups.isEmpty())
            backups = QDir(QDir::currentPath()).filePath("backups");

        const dm::PrunePlan plan = dm::SnapshotRetention::plan(backups, pol);
        for (const QString& k : plan.keep)
            err << "  keep   " << k << "\n";
        for (const QString& p : plan.prune)
            err << "  PRUNE  " << QDir(p).dirName() << "\n";
        err << plan.keep.size() << " kept, " << plan.prune.size() << " to prune ("
            << (plan.pruneBytes / 1024) << " KB)\n";

        if (!parser.isSet(applyOpt)) {
            err << "\n(dry run - pass --apply to delete)\n";
            return 0;
        }
        if (plan.prune.isEmpty())
            return 0;
        const dm::PruneResult r = dm::SnapshotRetention::apply(plan);
        err << (r.ok ? "OK   " : "FAIL ") << "removed " << r.removed << " ("
            << (r.bytes / 1024) << " KB)\n";
        for (const QString& e : r.errors)
            err << "  err : " << e << "\n";
        return r.ok ? 0 : 2;
    }

    // ---- verify mode -------------------------------------------------
    if (parser.isSet(verifyOpt)) {
        const dm::VerifyResult v =
            dm::SnapshotVerify::check(parser.value(verifyOpt));
        if (!v.error.isEmpty()) {
            err << "error: " << v.error << "\n";
            return 1;
        }
        for (const auto& c : v.components)
            err << "  " << c.status << "  " << c.name << "\n";
        err << (v.ok ? "OK  " : "BAD ") << v.okCount << " ok, " << v.badCount
            << " bad, " << v.skippedCount << " skipped\n";
        return v.ok ? 0 : 2;
    }

    // ---- diff mode -------------------------------------------------
    if (parser.isSet(diffOpt) || parser.isSet(diffToOpt)) {
        if (!parser.isSet(diffOpt) || !parser.isSet(diffToOpt)) {
            err << "error: --diff needs both <dir-a> and --diff-to <dir-b>\n";
            return 1;
        }
        const dm::DiffResult d = dm::SnapshotDiff::compare(
            parser.value(diffOpt), parser.value(diffToOpt));
        if (!d.error.isEmpty()) {
            err << "error: " << d.error << "\n";
            return 1;
        }
        err << "diff " << d.stampA << " -> " << d.stampB << "\n";
        for (const auto& x : d.deltas)
            err << "  " << x.change << "  " << x.name
                << (x.note.isEmpty() ? QString() : "  (" + x.note + ")") << "\n";
        err << d.added << " added, " << d.removed << " removed, " << d.changed
            << " changed\n";
        return 0;
    }

    // ---- bundle / unbundle -------------------------------------------
    if (parser.isSet(bundleOpt)) {
        const dm::BundleResult b = dm::SnapshotBundle::pack(
            parser.value(bundleOpt), parser.value(bundleOutOpt));
        if (!b.ok) {
            err << "error: " << b.error << "\n";
            return 1;
        }
        err << "bundled -> " << b.path << "  (" << (b.bytes / 1024) << " KB)\n";
        return 0;
    }
    if (parser.isSet(unbundleOpt)) {
        const QString dir = parser.isSet(unbundleOutOpt)
                                ? parser.value(unbundleOutOpt)
                                : QDir::currentPath();
        const dm::BundleResult b =
            dm::SnapshotBundle::unpack(parser.value(unbundleOpt), dir);
        if (!b.ok) {
            err << "error: " << b.error << "\n";
            return 1;
        }
        err << "extracted -> " << b.path << "\n"
            << "restore with:  devmanager-scan --restore \"" << b.path << "\"\n";
        return 0;
    }

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
