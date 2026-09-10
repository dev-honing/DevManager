#include "scan/skill_scanner.h"
#include "process_runner.h"

#include <QtTest>
#include <QTemporaryDir>
#include <QDir>

using namespace dm;

class TstSkillScanner : public QObject {
    Q_OBJECT

private slots:
    void classify();
    void internalPluginDirs();
    void scanRootsClassifiesAndFindsSkillMd();
    void mergeCollapsesSameNameAcrossRoots();
    void junctionIsDetectedAndClassifiedLinked();
};

static void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
}

void TstSkillScanner::classify()
{
    LinkInfo none;
    LinkInfo link;
    link.isLink = true;
    link.linkType = "Junction";

    QCOMPARE(SkillScanner::classifySkill("gpt-image", link), QStringLiteral("linked"));
    QCOMPARE(SkillScanner::classifySkill(".system", none), QStringLiteral("system"));
    QCOMPARE(SkillScanner::classifySkill("graphify", none), QStringLiteral("user"));
}

void TstSkillScanner::internalPluginDirs()
{
    QVERIFY(SkillScanner::isInternalPluginDir(".git"));
    QVERIFY(SkillScanner::isInternalPluginDir("cache"));
    QVERIFY(SkillScanner::isInternalPluginDir("MARKETPLACES"));
    QVERIFY(!SkillScanner::isInternalPluginDir("ponytail"));
}

void TstSkillScanner::scanRootsClassifiesAndFindsSkillMd()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.path() + "/.claude/skills";

    touch(root + "/alpha/SKILL.md");
    QVERIFY(QDir().mkpath(root + "/.hidden"));
    QVERIFY(QDir().mkpath(root + "/beta"));   // no SKILL.md

    const auto locs = SkillScanner::scanRoots({QDir::toNativeSeparators(root)},
                                              SkillScanner::Skills);
    QCOMPARE(locs.size(), 3);

    QMap<QString, ItemLocation> byName;
    for (const auto& l : locs)
        byName.insert(l.name, l);

    QVERIFY(byName.contains("alpha"));
    QCOMPARE(byName["alpha"].classification, QStringLiteral("user"));
    QVERIFY(byName["alpha"].hasSkillMd);
    QCOMPARE(byName["alpha"].host, QStringLiteral("claude"));

    QCOMPARE(byName[".hidden"].classification, QStringLiteral("system"));
    QVERIFY(!byName["beta"].hasSkillMd);
}

void TstSkillScanner::mergeCollapsesSameNameAcrossRoots()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString r1 = tmp.path() + "/a/skills";
    const QString r2 = tmp.path() + "/b/skills";
    touch(r1 + "/graphify/SKILL.md");
    touch(r2 + "/graphify/SKILL.md");
    touch(r2 + "/solo/SKILL.md");

    const auto locs = SkillScanner::scanRoots(
        {QDir::toNativeSeparators(r1), QDir::toNativeSeparators(r2)},
        SkillScanner::Skills);
    const auto merged = SkillScanner::mergeByName(locs);

    QCOMPARE(merged.size(), 2);
    QMap<QString, NamedItem> byName;
    for (const auto& m : merged)
        byName.insert(m.name, m);
    QCOMPARE(byName["graphify"].locations.size(), 2);
    QCOMPARE(byName["solo"].locations.size(), 1);
}

void TstSkillScanner::junctionIsDetectedAndClassifiedLinked()
{
#ifndef Q_OS_WIN
    QSKIP("junction test is Windows-only");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString canon = tmp.path() + "/canon/gpt-image";
    touch(canon + "/SKILL.md");
    const QString root = tmp.path() + "/.agents/skills";
    QVERIFY(QDir().mkpath(root));

    const QString linkPath = QDir::toNativeSeparators(root + "/gpt-image");
    const ProcessResult mk = ProcessRunner::run(
        "cmd", {"/c", "mklink", "/J", linkPath, QDir::toNativeSeparators(canon)}, 5000);
    if (!mk.ok())
        QSKIP("could not create a junction in this environment");

    const auto locs = SkillScanner::scanRoots({QDir::toNativeSeparators(root)},
                                              SkillScanner::Skills);
    QCOMPARE(locs.size(), 1);
    QVERIFY(locs[0].link.isLink);
    QCOMPARE(locs[0].link.linkType, QStringLiteral("Junction"));
    QCOMPARE(locs[0].classification, QStringLiteral("linked"));
    QVERIFY(!locs[0].link.target.isEmpty());
    QVERIFY(locs[0].link.target.contains("gpt-image"));
    QVERIFY(QFileInfo::exists(locs[0].link.target));
#endif
}

QTEST_MAIN(TstSkillScanner)
#include "tst_skill_scanner.moc"
