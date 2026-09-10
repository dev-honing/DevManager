#include "path_util.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QProcessEnvironment>

namespace dm::path {

QString homeDir()
{
    const auto env = QProcessEnvironment::systemEnvironment();
    QString h = env.value("USERPROFILE");
    if (h.isEmpty())
        h = QDir::homePath();
    return QDir::fromNativeSeparators(h);
}

QString normalizeLower(const QString& p)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(p)).toLower();
}

LinkInfo probeLink(const QString& path)
{
    LinkInfo r;
    QFileInfo fi(path);
    if (!fi.exists())
        return r;

    QString t;
    if (fi.isJunction()) {
        r.isLink = true;
        r.linkType = QStringLiteral("Junction");
        t = fi.junctionTarget();
    } else if (fi.isSymbolicLink()) {
        r.isLink = true;
        r.linkType = QStringLiteral("SymbolicLink");
        t = fi.symLinkTarget();
    }
    if (r.isLink && t.isEmpty())
        t = fi.symLinkTarget();     // fallback for odd reparse tags
    if (!t.isEmpty())
        r.target = QDir::toNativeSeparators(QDir::cleanPath(t));
    return r;
}

QString hostFromRoot(const QString& rootPath)
{
    const QString lower = normalizeLower(rootPath);
    if (lower.contains("/.claude/") || lower.endsWith("/.claude"))  return "claude";
    if (lower.contains("/.codex/")  || lower.endsWith("/.codex"))   return "codex";
    if (lower.contains("/.agents/") || lower.endsWith("/.agents"))  return "agents";
    if (lower.contains("/.gemini/") || lower.endsWith("/.gemini"))  return "gemini";

    const QFileInfo parent(QFileInfo(rootPath).absolutePath());
    return parent.fileName().isEmpty() ? QStringLiteral("unknown") : parent.fileName();
}

QStringList discoveryRoots(const QString& leaf)
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString home = homeDir();
    QSet<QString> seen;
    QStringList out;

    auto add = [&](const QString& candidate) {
        QFileInfo fi(candidate);
        if (!fi.exists() || !fi.isDir())
            return;
        const QString full = QDir::cleanPath(fi.absoluteFilePath());
        const QString key = full.toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            out << QDir::toNativeSeparators(full);
        }
    };

    for (const QString& base : {".claude", ".codex", ".agents", ".gemini",
                                ".gemini/config", ".config", ".local"})
        add(home + "/" + base + "/" + leaf);

    QStringList parents{home};
    for (const char* k : {"APPDATA", "LOCALAPPDATA"}) {
        const QString v = env.value(k);
        if (!v.isEmpty())
            parents << QDir::fromNativeSeparators(v);
    }
    for (const QString& parent : parents) {
        QDir d(parent);
        if (!d.exists())
            continue;
        for (const QFileInfo& child :
             d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
            add(child.absoluteFilePath() + "/" + leaf);
            add(child.absoluteFilePath() + "/config/" + leaf);
        }
    }

    out.sort(Qt::CaseInsensitive);
    return out;
}

} // namespace dm::path
