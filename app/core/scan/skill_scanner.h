#pragma once
#include "model.h"
#include <QStringList>

namespace dm {

struct DiscoveryResult {
    QStringList roots;
    QList<NamedItem> items;   // merged by logical name
};

// Ports Get-DynamicItems + Merge-DynamicItemsByName from snapshot-current.ps1.
// Read-only: walks discovery roots, inspects link + git + SKILL.md per entry.
class SkillScanner {
public:
    enum Mode { Skills, Plugins };

    static DiscoveryResult scan(Mode mode);

    // building blocks, exposed for tests
    static QList<ItemLocation> scanRoots(const QStringList& roots, Mode mode);
    static QList<NamedItem> mergeByName(const QList<ItemLocation>& locations);
    static QString classifySkill(const QString& name, const LinkInfo& link);
    static bool isInternalPluginDir(const QString& name);
    static GitInfo probeGit(const QString& path);
};

} // namespace dm
