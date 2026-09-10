#include "config/scan_config.h"

#include "json_io.h"
#include "path_util.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace dm {

// ---------------------------------------------------------------- defaults

ScanConfig ScanConfig::defaults()
{
    ScanConfig c;
    c.tools = {
        {"docker", "containers", {"--version"}, {}},
        {"node", "runtimes", {"--version"}, {}},
        {"npm", "runtimes", {"--version"}, {}},
        {"python", "runtimes", {"--version"}, {}},
        {"cmake", "build", {"--version"}, {}},
        {"git", "build", {"--version"}, {}},
        {"claude", "ai", {"--version"}, {}},
        {"codex", "ai", {"--version"}, {}},
        {"gemini", "ai", {"--version"}, {}},
        {"cursor-agent", "ai", {"--version"}, {}},
        {"aider", "ai", {"--version"}, {}},
        {"ollama", "ai", {"--version"}, {}},
        {"headroom", "ai", {"--version"}, {"~/.local/bin/headroom.exe"}},
        {"omniroute", "ai", {"--version"}, {}},
    };
    c.services = {
        {"headroom", "Headroom", 8787, {}, false},
        {"omniroute", "OmniRoute", 20128, {}, false},
        {"ollama", "Ollama", 11434, {}, false},
        {"docker", "Docker", 0, {"docker", "version", "--format", "{{.Server.Version}}"}, false},
        {"wsl", "WSL", 0, {}, true},
    };
    c.packageManagers = {
        {"npm", {}, {"ls", "-g", "--depth=0", "--json"}, "npm-deps"},
        {"pip", {}, {"-m", "pip", "list", "--format=json"}, "pip-list"},
    };
    c.envInclude = {"ANTHROPIC", "CLAUDE", "CODEX", "OMNIROUTE", "HEADROOM",
                    "DOCKER", "QT", "CMAKE", "GEMINI", "OPENAI", "OLLAMA",
                    "CURSOR", "AIDER", "GROQ", "MISTRAL", "DEEPSEEK", "XAI",
                    "GOOGLE_API", "GEMINI_API", "LLM", "AI_"};
    c.envSecret = {"KEY", "TOKEN", "SECRET", "PASSWORD", "PASSWD",
                   "CREDENTIAL", "AUTH", "PRIVATE"};
    c.qtSearchPaths = {"C:/Qt", "~/Qt", "$QTDIR", "$QT_ROOT", "/opt/Qt"};
    return c;
}

QStringList ScanConfig::categories() const
{
    QStringList out;
    for (const ToolSpec& t : tools)
        if (!out.contains(t.category))
            out << t.category;
    return out;
}

// ---------------------------------------------------------------- load

static QString findConfigFile()
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString override = env.value("DEVMANAGER_CONFIG_DIR");
    if (!override.isEmpty()) {
        const QString p = QDir(override).filePath("scan.json");
        if (QFileInfo::exists(p))
            return p;
    }
    // walk up from cwd, then from the executable dir, looking for config/scan.json
    for (const QString& start : {QDir::currentPath(),
                                 QCoreApplication::applicationDirPath()}) {
        QDir d(start);
        for (int i = 0; i < 6; ++i) {
            const QString p = d.filePath("config/scan.json");
            if (QFileInfo::exists(p))
                return QDir(p).absolutePath();
            if (!d.cdUp())
                break;
        }
    }
    return {};
}

static QStringList jsonStrings(const QJsonValue& v)
{
    QStringList out;
    for (const QJsonValue& x : v.toArray())
        out << x.toString();
    return out;
}

ScanConfig ScanConfig::load()
{
    ScanConfig c = defaults();

    const QString file = findConfigFile();
    if (file.isEmpty())
        return c;

    const QJsonObject root = json::read(file);
    if (root.isEmpty())
        return c;
    c.sourcePath = file;

    if (root.contains("tools")) {
        c.tools.clear();
        for (const QJsonValue& v : root.value("tools").toArray()) {
            const QJsonObject o = v.toObject();
            ToolSpec t;
            t.id = o.value("id").toString();
            t.category = o.value("category").toString("other");
            t.versionArgs = o.contains("versionArgs")
                                ? jsonStrings(o.value("versionArgs"))
                                : QStringList{"--version"};
            t.fallbackPaths = jsonStrings(o.value("fallbackPaths"));
            if (!t.id.isEmpty())
                c.tools << t;
        }
    }
    if (root.contains("services")) {
        c.services.clear();
        for (const QJsonValue& v : root.value("services").toArray()) {
            const QJsonObject o = v.toObject();
            ServiceSpec s;
            s.id = o.value("id").toString();
            s.name = o.value("name").toString(s.id);
            s.port = o.value("port").toInt(0);
            s.cliCheck = jsonStrings(o.value("cliCheck"));
            s.wslRunning = o.value("wslRunning").toBool(false);
            if (!s.id.isEmpty())
                c.services << s;
        }
    }
    if (root.contains("packageManagers")) {
        c.packageManagers.clear();
        for (const QJsonValue& v : root.value("packageManagers").toArray()) {
            const QJsonObject o = v.toObject();
            PackageManagerSpec m;
            m.id = o.value("id").toString();
            m.listArgs = jsonStrings(o.value("listArgs"));
            m.parseMode = o.value("parseMode").toString();
            if (!m.id.isEmpty())
                c.packageManagers << m;
        }
    }
    if (root.contains("envInclude"))
        c.envInclude = jsonStrings(root.value("envInclude"));
    if (root.contains("envSecret"))
        c.envSecret = jsonStrings(root.value("envSecret"));
    if (root.contains("qtSearchPaths"))
        c.qtSearchPaths = jsonStrings(root.value("qtSearchPaths"));

    return c;
}

} // namespace dm
