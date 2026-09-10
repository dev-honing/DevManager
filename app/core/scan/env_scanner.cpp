#include "scan/env_scanner.h"

#include "path_util.h"
#include "process_runner.h"
#include "scan/skill_scanner.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>

namespace dm {

static QString resolveExe(const QString& name)
{
    const QString found = QStandardPaths::findExecutable(name);
    return found.isEmpty() ? name : found;
}

static QString versionOf(const QString& exe, const QStringList& args = {"--version"})
{
    const ProcessResult r = ProcessRunner::run(resolveExe(exe), args, 8000);
    return r.started ? r.firstLine() : QString();
}

QMap<QString, QString> EnvironmentScanner::scanTools()
{
    QMap<QString, QString> t;
    for (const char* name : {"docker", "node", "npm", "python", "cmake", "git",
                             "claude", "codex"})
        t.insert(name, versionOf(name));

    QString headroom = QStandardPaths::findExecutable("headroom");
    if (headroom.isEmpty()) {
        const QString fb = path::homeDir() + "/.local/bin/headroom.exe";
        if (QFileInfo::exists(fb))
            headroom = fb;
    }
    if (!headroom.isEmpty())
        t.insert("headroom", ProcessRunner::run(headroom, {"--version"}, 8000).firstLine());

    t.insert("omniroute", versionOf("omniroute"));
    return t;
}

QString EnvironmentScanner::scanQt()
{
    QDir qt("C:/Qt");
    if (!qt.exists())
        return {};
    static const QRegularExpression re("^\\d+\\.\\d+");
    QStringList versions;
    for (const QString& d : qt.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (re.match(d).hasMatch())
            versions << d;
    }
    return versions.join(", ");
}

QString EnvironmentScanner::scanVisualStudio()
{
    const QString pfx86 =
        QProcessEnvironment::systemEnvironment().value("ProgramFiles(x86)");
    if (pfx86.isEmpty())
        return {};
    const QString vswhere =
        pfx86 + "/Microsoft Visual Studio/Installer/vswhere.exe";
    if (!QFileInfo::exists(vswhere))
        return {};
    const ProcessResult r = ProcessRunner::run(
        vswhere, {"-all", "-property", "catalog_productDisplayVersion"}, 8000);
    if (!r.started)
        return {};
    QStringList v;
    for (const QString& line : r.outText().split('\n', Qt::SkipEmptyParts))
        v << line.trimmed();
    return v.join(", ");
}

WslInfo EnvironmentScanner::scanWsl()
{
    WslInfo info;
    const QString wsl = QStandardPaths::findExecutable("wsl");
    if (wsl.isEmpty())
        return info;

    const ProcessResult r = ProcessRunner::run(wsl, {"-l", "-v"}, 8000);
    if (!r.started)
        return info;
    info.installed = true;

    // `wsl -l -v` emits UTF-16LE.
    QString text = QString::fromUtf16(
        reinterpret_cast<const char16_t*>(r.out.constData()), r.out.size() / 2);

    const QStringList lines = text.split(QRegularExpression("[\\r\\n]+"),
                                         Qt::SkipEmptyParts);
    for (int i = 1; i < lines.size(); ++i) {   // line 0 is the header
        QString line = lines.at(i);
        line.replace('*', ' ');
        const QStringList cols =
            line.split(QRegularExpression("\\s{2,}|\\t"), Qt::SkipEmptyParts);
        if (cols.size() < 3)
            continue;
        WslDistro d;
        d.name = cols.at(0).trimmed();
        d.state = cols.at(1).trimmed();
        d.version = cols.at(2).trimmed().toInt();
        if (!d.name.isEmpty())
            info.distributions.append(d);
    }
    return info;
}

QList<Package> EnvironmentScanner::scanGlobalPackages()
{
    QList<Package> pkgs;

    // npm ls -g --depth=0 --json  ->  .dependencies
    {
        const QString npm = QStandardPaths::findExecutable("npm");
        if (!npm.isEmpty()) {
            const ProcessResult r =
                ProcessRunner::run(npm, {"ls", "-g", "--depth=0", "--json"}, 20000);
            const QJsonObject deps =
                QJsonDocument::fromJson(r.out).object().value("dependencies").toObject();
            QList<Package> npmPkgs;
            for (auto it = deps.constBegin(); it != deps.constEnd(); ++it) {
                Package p;
                p.manager = "npm";
                p.name = it.key();
                p.version = it.value().toObject().value("version").toString();
                p.restorePolicy = "inventory-only";
                npmPkgs.append(p);
            }
            std::sort(npmPkgs.begin(), npmPkgs.end(),
                      [](const Package& a, const Package& b) { return a.name < b.name; });
            pkgs += npmPkgs;
        }
    }

    // python -m pip list --format=json
    {
        const QString py = QStandardPaths::findExecutable("python");
        if (!py.isEmpty()) {
            const ProcessResult r =
                ProcessRunner::run(py, {"-m", "pip", "list", "--format=json"}, 20000);
            QList<Package> pipPkgs;
            for (const QJsonValue& v : QJsonDocument::fromJson(r.out).array()) {
                const QJsonObject o = v.toObject();
                Package p;
                p.manager = "pip";
                p.name = o.value("name").toString();
                p.version = o.value("version").toString();
                p.restorePolicy = "inventory-only";
                pipPkgs.append(p);
            }
            std::sort(pipPkgs.begin(), pipPkgs.end(),
                      [](const Package& a, const Package& b) { return a.name < b.name; });
            pkgs += pipPkgs;
        }
    }

    return pkgs;
}

bool EnvironmentScanner::isSecretName(const QString& name)
{
    static const QRegularExpression re(
        "KEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIAL|AUTH|PRIVATE",
        QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch();
}

QString EnvironmentScanner::maskSecret(const QString& value)
{
    if (value.isEmpty())
        return value;
    if (value.size() <= 8)
        return "***";
    return value.left(4) + "..." + value.right(4);
}

QMap<QString, QString> EnvironmentScanner::scanEnv()
{
    // Case-insensitive to match PowerShell's -match semantics (v4.1).
    static const QRegularExpression keep(
        "ANTHROPIC|CLAUDE|CODEX|OMNIROUTE|HEADROOM|DOCKER|QT|CMAKE|GEMINI",
        QRegularExpression::CaseInsensitiveOption);
    QMap<QString, QString> out;
    const auto env = QProcessEnvironment::systemEnvironment();
    for (const QString& name : env.keys()) {
        if (!keep.match(name).hasMatch())
            continue;
        QString v = env.value(name);
        if (isSecretName(name))
            v = maskSecret(v);
        out.insert(name, v);
    }
    return out;
}

EnvironmentInventory EnvironmentScanner::scan()
{
    EnvironmentInventory inv;
    const auto env = QProcessEnvironment::systemEnvironment();

    inv.capturedAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    inv.machine = env.value("COMPUTERNAME");
    inv.userProfile = QDir::toNativeSeparators(path::homeDir());
    inv.root = QDir::toNativeSeparators(QDir::currentPath());

    inv.osCaption = QSysInfo::prettyProductName();
    inv.osVersion = QSysInfo::productVersion();
    inv.os64Bit = QSysInfo::currentCpuArchitecture().contains("64");

    inv.tools = scanTools();
    inv.qt = scanQt();
    inv.visualStudio = scanVisualStudio();
    inv.wsl = scanWsl();

    const DiscoveryResult skills = SkillScanner::scan(SkillScanner::Skills);
    const DiscoveryResult plugins = SkillScanner::scan(SkillScanner::Plugins);
    inv.skillRoots = skills.roots;
    inv.pluginRoots = plugins.roots;
    inv.skills = skills.items;
    inv.plugins = plugins.items;
    inv.globalPackages = scanGlobalPackages();

    inv.env = scanEnv();
    return inv;
}

} // namespace dm
