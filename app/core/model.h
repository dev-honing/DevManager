#pragma once
//
// DevManager environment inventory data model (Phase 1).
//
// This mirrors the shape produced by scripts/snapshot-current.ps1 v4.1
// (inventory.json) closely enough for a semantic regression diff, while
// bumping schemaVersion and dropping the UTF-8 BOM.
//
// inventory  = "how this machine was configured"  (description / validation)
// manifest   = "what a snapshot contains and how to restore it"  (later phase)
//
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QList>
#include <QMap>

namespace dm {

struct LinkInfo {
    bool isLink = false;
    QString linkType;   // "Junction" | "SymbolicLink" | null
    QString target;     // resolved absolute path, or null
    QJsonObject toJson() const;
};

struct GitInfo {
    bool detected = false;
    QString root;
    QString remote;
    QString commit;
    QString branch;
    QJsonObject toJson() const;
};

// One physical place a skill/plugin of a given logical name lives.
struct ItemLocation {
    QString name;             // logical item name (not serialized here; lives on NamedItem)
    QString host;             // "claude" | "codex" | "agents" | "gemini" | <parent dir>
    QString classification;   // skills: "user" | "system" | "linked";  plugins: "user"
    QString root;             // discovery root that contained it
    QString path;             // absolute path of the item
    bool hasSkillMd = false;
    LinkInfo link;
    GitInfo git;
    QJsonObject toJson() const;
};

// A logical skill/plugin name with every location it was found at.
struct NamedItem {
    QString name;
    QList<ItemLocation> locations;
    QJsonObject toJson() const;
};

struct Package {
    QString manager;        // "npm" | "pip"
    QString name;
    QString version;
    QString restorePolicy;  // always "inventory-only" in Phase 1
    QJsonObject toJson() const;
};

struct WslDistro {
    QString name;
    QString state;
    int version = 0;
    QJsonObject toJson() const;
};

struct WslInfo {
    bool installed = false;
    QList<WslDistro> distributions;
    QJsonObject toJson() const;
};

struct EnvironmentInventory {
    int schemaVersion = 5;
    QString snapshotVersion = QStringLiteral("5.0-dev");
    QString producedBy = QStringLiteral("DevManager-scan/0.1 (C++/Qt6)");
    QString capturedAt;     // ISO-8601
    QString machine;
    QString userProfile;
    QString root;

    QString osCaption;
    QString osVersion;
    bool os64Bit = true;

    QMap<QString, QString> tools;   // docker,node,npm,python,cmake,git,claude,codex,headroom,omniroute
    QMap<QString, QString> toolPaths;   // same keys -> resolved executable path (UI only)
    QString qt;
    QString visualStudio;
    WslInfo wsl;

    QStringList skillRoots;
    QStringList pluginRoots;
    QList<NamedItem> skills;
    QList<NamedItem> plugins;
    QList<Package> globalPackages;

    QMap<QString, QString> env;     // filtered + secret-masked

    QJsonObject toJson() const;
};

} // namespace dm
