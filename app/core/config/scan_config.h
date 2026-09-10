#pragma once
//
// User-tunable scan targets. Everyone runs a different set of AI CLIs,
// local services, package managers and env-var conventions, so none of
// that is baked into code -- it comes from config/scan.json, with these
// defaults() as the fallback when the file is absent.
//
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace dm {

struct ToolSpec {
    QString id;                 // command name, e.g. "claude", "ollama"
    QString category;           // "ai" | "runtimes" | "build" | "containers" | ...
    QStringList versionArgs{"--version"};
    QStringList fallbackPaths;  // absolute/`~`-relative, tried if not on PATH
};

struct ServiceSpec {
    QString id;
    QString name;               // display
    int port = 0;               // >0 => TCP liveness check on 127.0.0.1:port
    QStringList cliCheck;        // non-empty => run it; exit 0 + output => running
    bool wslRunning = false;     // true => `wsl -l -q --running` non-empty => running
};

struct PackageManagerSpec {
    QString id;                 // "npm", "pip", "pnpm", ...
    QString exe;                // resolved on PATH
    QStringList listArgs;
    QString parseMode;          // "npm-deps" | "pip-list"
};

// One snapshot-policy rule. First match wins (see SnapshotPlanner).
struct SnapshotRule {
    QString root;               // "" = any backup root, else its dir name (".claude")
    QString name;               // "" = any top-level entry, else exact match
    QString policy;             // "backup" | "inventory-only" | "regenerate" | "exclude"
    QString reason;
};

struct ScanConfig {
    QList<ToolSpec> tools;
    QList<ServiceSpec> services;
    QList<PackageManagerSpec> packageManagers;
    QStringList envInclude;      // name substrings (case-insensitive) to surface
    QStringList envSecret;       // name substrings that force value masking
    QStringList qtSearchPaths;   // dirs scanned for `\d+\.\d+` version subfolders
    QStringList backupRoots;     // dirs a snapshot would consider (`~`-relative ok)
    QList<SnapshotRule> snapshotRules;              // per-entry policy, first match wins
    QMap<QString, QString> snapshotRootDefaults;    // rootId -> policy; "*" = catch-all
    QString sourcePath;          // file it was loaded from; empty => built-in defaults

    static ScanConfig defaults();
    static ScanConfig load();    // find config/scan.json, merge onto defaults()
    QStringList categories() const;   // distinct tool categories, in first-seen order
};

} // namespace dm
