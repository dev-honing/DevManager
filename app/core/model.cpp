#include "model.h"

namespace dm {

static QJsonValue orNull(const QString& s)
{
    return s.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s);
}

QJsonObject LinkInfo::toJson() const
{
    return {
        {"isLink", isLink},
        {"linkType", orNull(linkType)},
        {"target", orNull(target)},
    };
}

QJsonObject GitInfo::toJson() const
{
    return {
        {"detected", detected},
        {"root", orNull(root)},
        {"remote", orNull(remote)},
        {"commit", orNull(commit)},
        {"branch", orNull(branch)},
    };
}

QJsonObject ItemLocation::toJson() const
{
    return {
        {"host", host},
        {"classification", classification},
        {"root", root},
        {"path", path},
        {"hasSkillMd", hasSkillMd},
        {"link", link.toJson()},
        {"git", git.toJson()},
    };
}

QJsonObject NamedItem::toJson() const
{
    QJsonArray locs;
    for (const auto& l : locations)
        locs.append(l.toJson());
    return {{"name", name}, {"locations", locs}};
}

QJsonObject Package::toJson() const
{
    return {
        {"manager", manager},
        {"name", name},
        {"version", version},
        {"restorePolicy", restorePolicy},
    };
}

QJsonObject WslDistro::toJson() const
{
    return {{"name", name}, {"state", state}, {"version", version}};
}

QJsonObject WslInfo::toJson() const
{
    QJsonArray d;
    for (const auto& x : distributions)
        d.append(x.toJson());
    return {{"installed", installed}, {"distributions", d}};
}

static QJsonObject mapToJson(const QMap<QString, QString>& m)
{
    QJsonObject o;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

static QJsonArray namedItems(const QList<NamedItem>& items)
{
    QJsonArray a;
    for (const auto& i : items)
        a.append(i.toJson());
    return a;
}

QJsonObject EnvironmentInventory::toJson() const
{
    QJsonArray pkgs;
    for (const auto& p : globalPackages)
        pkgs.append(p.toJson());

    QJsonObject discovery{
        {"skillRoots", QJsonArray::fromStringList(skillRoots)},
        {"pluginRoots", QJsonArray::fromStringList(pluginRoots)},
        {"skills", namedItems(skills)},
        {"plugins", namedItems(plugins)},
        {"globalPackages", pkgs},
    };

    return {
        {"schemaVersion", schemaVersion},
        {"snapshotVersion", snapshotVersion},
        {"producedBy", producedBy},
        {"capturedAt", capturedAt},
        {"machine", machine},
        {"userProfile", userProfile},
        {"root", root},
        {"os", QJsonObject{
                   {"caption", osCaption},
                   {"version", osVersion},
                   {"is64Bit", os64Bit},
               }},
        {"tools", mapToJson(tools)},
        {"qt", qt.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(qt)},
        {"visualStudio", visualStudio.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                : QJsonValue(visualStudio)},
        {"wsl", wsl.toJson()},
        {"discovery", discovery},
        {"env", mapToJson(env)},
    };
}

} // namespace dm
