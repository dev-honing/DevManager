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
#include <QSet>
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

EnvironmentScanner::ToolScan EnvironmentScanner::scanTools(const ScanConfig& cfg)
{
    ToolScan out;
    for (const ToolSpec& spec : cfg.tools) {
        QString exe = QStandardPaths::findExecutable(spec.id);
        if (exe.isEmpty()) {
            for (const QString& fb : spec.fallbackPaths) {
                const QString p = path::expand(fb);
                if (!p.isEmpty() && QFileInfo::exists(p)) {
                    exe = p;
                    break;
                }
            }
        }
        if (exe.isEmpty())
            continue;   // not installed -> not listed

        const ProcessResult r = ProcessRunner::run(exe, spec.versionArgs, 8000);
        QString ver;
        if (r.started) {
            // Some CLIs (e.g. ollama with its server down) emit warning lines
            // around the real version. Prefer a line that carries a version
            // number, then any non-warning line, then whatever came first.
            static const QRegularExpression verLike("\\d+\\.\\d+");
            const QStringList lines = r.outText().split('\n', Qt::SkipEmptyParts);
            QString firstNonWarning;
            for (const QString& line : lines) {
                const QString t = line.trimmed();
                const bool warn = t.startsWith("warning", Qt::CaseInsensitive)
                                  || t.startsWith("error", Qt::CaseInsensitive);
                if (verLike.match(t).hasMatch()) { ver = t; break; }
                if (!warn && firstNonWarning.isEmpty())
                    firstNonWarning = t;
            }
            if (ver.isEmpty())
                ver = !firstNonWarning.isEmpty() ? firstNonWarning : r.firstLine();
            ver.remove(QRegularExpression("^(warning|error):\\s*",
                                          QRegularExpression::CaseInsensitiveOption));
        }
        out.versions.insert(spec.id, ver);
        out.paths.insert(spec.id, QDir::toNativeSeparators(exe));
        out.categories.insert(spec.id, spec.category);
    }
    return out;
}

QString EnvironmentScanner::scanQt(const ScanConfig& cfg)
{
    static const QRegularExpression re("^\\d+\\.\\d+");
    QStringList versions;
    QSet<QString> seen;
    for (const QString& raw : cfg.qtSearchPaths) {
        const QString dirPath = path::expand(raw);
        if (dirPath.isEmpty())
            continue;
        QDir qt(dirPath);
        if (!qt.exists())
            continue;
        for (const QString& d : qt.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (re.match(d).hasMatch() && !seen.contains(d)) {
                seen.insert(d);
                versions << d;
            }
        }
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

QList<Package> EnvironmentScanner::scanGlobalPackages(const ScanConfig& cfg)
{
    QList<Package> pkgs;

    for (const PackageManagerSpec& m : cfg.packageManagers) {
        // "pip" is invoked as `python -m pip ...`; everything else by its own id.
        const QString exeName = (m.id == "pip") ? QStringLiteral("python") : m.id;
        const QString exe = QStandardPaths::findExecutable(exeName);
        if (exe.isEmpty())
            continue;

        const ProcessResult r = ProcessRunner::run(exe, m.listArgs, 20000);
        QList<Package> found;

        if (m.parseMode == "npm-deps") {
            const QJsonObject deps = QJsonDocument::fromJson(r.out)
                                         .object().value("dependencies").toObject();
            for (auto it = deps.constBegin(); it != deps.constEnd(); ++it)
                found.append({m.id, it.key(),
                              it.value().toObject().value("version").toString(),
                              "inventory-only"});
        } else if (m.parseMode == "pip-list") {
            for (const QJsonValue& v : QJsonDocument::fromJson(r.out).array()) {
                const QJsonObject o = v.toObject();
                found.append({m.id, o.value("name").toString(),
                              o.value("version").toString(), "inventory-only"});
            }
        }

        std::sort(found.begin(), found.end(),
                  [](const Package& a, const Package& b) { return a.name < b.name; });
        pkgs += found;
    }
    return pkgs;
}

bool EnvironmentScanner::isSecretName(const QString& name, const QStringList& patterns)
{
    if (patterns.isEmpty())
        return false;
    const QRegularExpression re(patterns.join('|'),
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

QMap<QString, QString> EnvironmentScanner::scanEnv(const ScanConfig& cfg)
{
    if (cfg.envInclude.isEmpty())
        return {};
    const QRegularExpression keep(cfg.envInclude.join('|'),
                                  QRegularExpression::CaseInsensitiveOption);
    QMap<QString, QString> out;
    const auto env = QProcessEnvironment::systemEnvironment();
    for (const QString& name : env.keys()) {
        if (!keep.match(name).hasMatch())
            continue;
        QString v = env.value(name);
        if (isSecretName(name, cfg.envSecret))
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

    const ScanConfig cfg = ScanConfig::load();
    inv.configSource = cfg.sourcePath;

    const ToolScan ts = scanTools(cfg);
    inv.tools = ts.versions;
    inv.toolPaths = ts.paths;
    inv.toolCategories = ts.categories;
    inv.qt = scanQt(cfg);
    inv.visualStudio = scanVisualStudio();
    inv.wsl = scanWsl();

    const DiscoveryResult skills = SkillScanner::scan(SkillScanner::Skills);
    const DiscoveryResult plugins = SkillScanner::scan(SkillScanner::Plugins);
    inv.skillRoots = skills.roots;
    inv.pluginRoots = plugins.roots;
    inv.skills = skills.items;
    inv.plugins = plugins.items;
    inv.globalPackages = scanGlobalPackages(cfg);

    inv.env = scanEnv(cfg);
    return inv;
}

} // namespace dm
