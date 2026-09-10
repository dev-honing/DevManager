#include "snapshot/snapshot_diff.h"

#include "json_io.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace dm {

namespace {

struct Comp {
    QString sha, policy, linkTarget;
    double size = 0;
    bool isLink = false;
};

// name -> component fields we compare on
QMap<QString, Comp> readComponents(const QJsonObject& manifest)
{
    QMap<QString, Comp> out;
    for (const QJsonValue& v : manifest.value("components").toArray()) {
        const QJsonObject c = v.toObject();
        Comp comp;
        comp.sha = c.value("sha256").toString();
        comp.policy = c.value("policy").toString();
        comp.isLink = c.value("type").toString() == "link";
        comp.linkTarget = c.value("linkTarget").toString();
        comp.size = c.value("sizeBytes").toDouble(-1);
        out.insert(c.value("name").toString(), comp);
    }
    return out;
}

// "" => equal; otherwise a short reason
QString diffOne(const Comp& a, const Comp& b)
{
    if (a.policy != b.policy)
        return "policy " + a.policy + " -> " + b.policy;
    if (a.isLink || b.isLink) {
        if (a.isLink != b.isLink)
            return a.isLink ? "link -> data" : "data -> link";
        return a.linkTarget == b.linkTarget ? QString()
                                            : "target " + a.linkTarget + " -> " + b.linkTarget;
    }
    if (!a.sha.isEmpty() && !b.sha.isEmpty())
        return a.sha == b.sha ? QString() : "content changed";
    if (a.size >= 0 && b.size >= 0)
        return qFuzzyCompare(a.size, b.size)
                   ? QString()
                   : QString("size %1 -> %2").arg(qint64(a.size)).arg(qint64(b.size));
    return {};   // nothing comparable recorded -> treat as unchanged
}

} // namespace

DiffResult SnapshotDiff::compare(const QString& dirA, const QString& dirB)
{
    DiffResult res;
    const QJsonObject ma = json::read(QDir(dirA).filePath("manifest.json"));
    const QJsonObject mb = json::read(QDir(dirB).filePath("manifest.json"));
    if (ma.isEmpty() || mb.isEmpty()) {
        res.error = ma.isEmpty() ? "manifest A missing or unreadable"
                                 : "manifest B missing or unreadable";
        return res;
    }
    res.stampA = ma.value("stamp").toString();
    res.stampB = mb.value("stamp").toString();

    const QMap<QString, Comp> a = readComponents(ma);
    const QMap<QString, Comp> b = readComponents(mb);

    QStringList names = a.keys();
    for (const QString& n : b.keys())
        if (!names.contains(n))
            names << n;
    names.sort();

    for (const QString& n : names) {
        const bool inA = a.contains(n), inB = b.contains(n);
        if (inA && !inB) {
            res.deltas << SnapshotDelta{n, "removed", {}};
            res.removed += 1;
        } else if (!inA && inB) {
            res.deltas << SnapshotDelta{n, "added", {}};
            res.added += 1;
        } else {
            const QString why = diffOne(a.value(n), b.value(n));
            if (!why.isEmpty()) {
                res.deltas << SnapshotDelta{n, "changed", why};
                res.changed += 1;
            }
        }
    }

    res.ok = true;
    return res;
}

} // namespace dm
