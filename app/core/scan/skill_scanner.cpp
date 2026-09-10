#include "scan/skill_scanner.h"

#include "path_util.h"
#include "process_runner.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

namespace dm {

GitInfo SkillScanner::probeGit(const QString& path)
{
    GitInfo g;
    if (path.isEmpty() || !QFileInfo::exists(path))
        return g;

    auto git = [&](const QStringList& args) {
        return ProcessRunner::run("git", QStringList{"-C", path} + args, 5000);
    };

    const ProcessResult top = git({"rev-parse", "--show-toplevel"});
    if (!top.ok())
        return g;
    g.root = top.firstLine();
    if (g.root.isEmpty())
        return g;
    g.detected = true;

    const ProcessResult remote = git({"config", "--get", "remote.origin.url"});
    if (remote.ok()) g.remote = remote.firstLine();

    const ProcessResult commit = git({"rev-parse", "HEAD"});
    if (commit.ok()) g.commit = commit.firstLine();

    const ProcessResult branch = git({"branch", "--show-current"});
    if (branch.ok()) g.branch = branch.firstLine();

    return g;
}

bool SkillScanner::isInternalPluginDir(const QString& name)
{
    if (name.startsWith('.'))
        return true;
    static const QStringList internal{
        "cache", "data", "config", "marketplaces", "logs",
        "tmp", "temp", "state", "sessions", "staging"};
    return internal.contains(name.toLower());
}

QString SkillScanner::classifySkill(const QString& name, const LinkInfo& link)
{
    if (link.isLink)
        return "linked";
    if (name.startsWith('.'))
        return "system";
    return "user";
}

QList<ItemLocation> SkillScanner::scanRoots(const QStringList& roots, Mode mode)
{
    QList<ItemLocation> out;

    for (const QString& rootPath : roots) {
        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QString host = path::hostFromRoot(rootPath);

        for (const QFileInfo& entry :
             root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
            const QString name = entry.fileName();
            if (mode == Plugins && isInternalPluginDir(name))
                continue;

            ItemLocation loc;
            loc.name = name;
            loc.host = host;
            loc.root = QDir::toNativeSeparators(rootPath);
            loc.path = QDir::toNativeSeparators(entry.absoluteFilePath());
            loc.link = path::probeLink(entry.absoluteFilePath());

            QString gitProbe = entry.absoluteFilePath();
            if (loc.link.isLink && !loc.link.target.isEmpty()
                && QFileInfo::exists(loc.link.target))
                gitProbe = loc.link.target;
            loc.git = probeGit(gitProbe);

            if (mode == Skills) {
                loc.hasSkillMd =
                    QFileInfo::exists(entry.absoluteFilePath() + "/SKILL.md");
                loc.classification = classifySkill(name, loc.link);
            } else {
                loc.classification = "user";
            }

            out.append(loc);
        }
    }
    return out;
}

QList<NamedItem> SkillScanner::mergeByName(const QList<ItemLocation>& locations)
{
    QList<NamedItem> result;
    QHash<QString, int> indexByKey;

    for (const ItemLocation& loc : locations) {
        const QString name =
            loc.name.isEmpty() ? QFileInfo(loc.path).fileName() : loc.name;
        const QString key = name.toLower();
        int idx = indexByKey.value(key, -1);
        if (idx < 0) {
            NamedItem ni;
            ni.name = name;
            result.append(ni);
            idx = result.size() - 1;
            indexByKey.insert(key, idx);
        }
        result[idx].locations.append(loc);
    }
    return result;
}

DiscoveryResult SkillScanner::scan(Mode mode)
{
    DiscoveryResult r;
    r.roots = path::discoveryRoots(mode == Skills ? "skills" : "plugins");
    r.items = mergeByName(scanRoots(r.roots, mode));
    return r;
}

} // namespace dm
