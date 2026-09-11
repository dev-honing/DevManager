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
        {"docker", "containers", {"--version"}, {}, "winget install --id Docker.DockerDesktop -e"},
        {"node", "runtimes", {"--version"}, {}, "winget install --id OpenJS.NodeJS.LTS -e"},
        {"npm", "runtimes", {"--version"}, {}, ""},
        {"python", "runtimes", {"--version"}, {}, "winget install --id Python.Python.3.12 -e"},
        {"cmake", "build", {"--version"}, {}, "winget install --id Kitware.CMake -e"},
        {"git", "build", {"--version"}, {}, "winget install --id Git.Git -e"},
        {"claude", "ai", {"--version"}, {}, "npm install -g @anthropic-ai/claude-code"},
        {"codex", "ai", {"--version"}, {}, "npm install -g @openai/codex"},
        {"gemini", "ai", {"--version"}, {}, "npm install -g @google/gemini-cli"},
        {"cursor-agent", "ai", {"--version"}, {}, ""},
        {"aider", "ai", {"--version"}, {}, "python -m pip install aider-install && aider-install"},
        {"ollama", "ai", {"--version"}, {}, "winget install --id Ollama.Ollama -e"},
        {"headroom", "ai", {"--version"}, {"~/.local/bin/headroom.exe"}, ""},
        {"omniroute", "ai", {"--version"}, {}, ""},
    };
    c.services = {
        {"headroom", "Headroom", 8787, {}, false,
         {"headroom",
          {"install", "status", "--profile", "init-user"},
          {"install", "stop", "--profile", "init-user"},
          {"install", "start", "--profile", "init-user"},
          {"install", "restart", "--profile", "init-user"},
          false},
         true, "ANTHROPIC_BASE_URL"},
        {"omniroute", "OmniRoute", 20128, {}, false,
         {"omniroute", {"status"}, {"stop"}, {"serve"}, {"restart"}, true},
         true, "ANTHROPIC_TARGET_API_URL"},
        {"ollama", "Ollama", 11434, {}, false, {}},
        {"docker", "Docker", 0, {"docker", "version", "--format", "{{.Server.Version}}"}, false, {}},
        {"wsl", "WSL", 0, {}, true, {}},
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
    c.backupRoots = {"~/.claude", "~/.codex", "~/.agents", "~/.gemini",
                     "~/.headroom", "~/.omniroute"};
    c.hostProfiles = {
        {"msvc-qt6", {"cmake", "git", "node"}},   // native C/C++ track (VS2026 + Qt verified manually)
    };
    c.snapshotRootDefaults = {{".claude", "exclude"},
                              {".codex", "exclude"},
                              {".headroom", "exclude"},
                              {".omniroute", "exclude"},
                              {"*", "backup"}};
    c.snapshotRules = {
        {".claude", "settings.json", "backup", "core config"},
        {".claude", "CLAUDE.md", "backup", "user rules"},
        {".claude", ".credentials.json", "exclude", "secret — re-login"},
        {".claude", "skills", "backup", "user skills"},
        {".claude", "plugins", "regenerate", "re-install from marketplace"},
        {".claude", "projects", "inventory-only", "session transcripts"},
        {".claude", "history.jsonl", "inventory-only", "prompt history"},
        {".claude", "cache", "exclude", "cache"},
        {".claude", "file-history", "exclude", "editor undo"},
        {".claude", "shell-snapshots", "exclude", "runtime"},
        {".codex", "config.toml", "backup", "core config"},
        {".codex", "AGENTS.md", "backup", "user rules"},
        {".codex", "hooks.json", "backup", "config"},
        {".codex", "auth.json", "exclude", "secret — re-login"},
        {".codex", "skills", "backup", "user skills"},
        {".codex", "plugins", "regenerate", "re-install"},
        {".codex", ".tmp", "exclude", "scratch"},
        {".codex", ".sandbox-bin", "exclude", "runtime"},
        {".codex", "sessions", "inventory-only", "session data"},
        {".codex", "cache", "exclude", "cache"},
        {".headroom", "deploy", "backup", "deployment definition"},
        {".headroom", "config", "backup", "config"},
        {".headroom", "logs", "exclude", "logs"},
        {".omniroute", ".env", "exclude", "secret — re-login"},
        {".omniroute", "storage.sqlite", "backup", "routing state"},
        {".omniroute", "call_logs", "exclude", "logs"},
        {".omniroute", "db_backups", "exclude", "logs"},
        {".omniroute", "logs", "exclude", "logs"},
        // generic hints that apply to any AI tool dir added to backupRoots
        {"", "logs", "exclude", "logs"},
        {"", "cache", "exclude", "cache"},
        {"", ".cache", "exclude", "cache"},
        {"", "tmp", "exclude", "scratch"},
        {"", ".tmp", "exclude", "scratch"},
        {"", "node_modules", "regenerate", "re-install"},
        {"", "auth.json", "exclude", "secret"},
        {"", ".env", "exclude", "secret"},
        {"", "credentials.json", "exclude", "secret"},
        {"", ".credentials.json", "exclude", "secret"},
    };
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
            t.install = o.value("install").toString();
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
            if (o.contains("lifecycle")) {
                const QJsonObject lc = o.value("lifecycle").toObject();
                s.lifecycle.exe = lc.value("exe").toString();
                s.lifecycle.statusArgs = jsonStrings(lc.value("status"));
                s.lifecycle.stopArgs = jsonStrings(lc.value("stop"));
                s.lifecycle.startArgs = jsonStrings(lc.value("start"));
                s.lifecycle.restartArgs = jsonStrings(lc.value("restart"));
                s.lifecycle.startDetached = lc.value("startDetached").toBool(true);
            }
            s.snapshotBlocker = o.value("snapshotBlocker").toBool(false);
            s.envIndicatorVar = o.value("envIndicatorVar").toString();
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
    if (root.contains("backupRoots"))
        c.backupRoots = jsonStrings(root.value("backupRoots"));

    if (root.contains("hostProfiles")) {
        c.hostProfiles.clear();
        const QJsonObject hp = root.value("hostProfiles").toObject();
        for (auto it = hp.constBegin(); it != hp.constEnd(); ++it)
            c.hostProfiles.insert(it.key(), jsonStrings(it.value().toObject().value("tools")));
    }

    if (root.contains("snapshot")) {
        const QJsonObject sn = root.value("snapshot").toObject();
        if (sn.contains("rootDefaults")) {
            c.snapshotRootDefaults.clear();
            const QJsonObject rd = sn.value("rootDefaults").toObject();
            for (auto it = rd.constBegin(); it != rd.constEnd(); ++it)
                c.snapshotRootDefaults.insert(it.key(), it.value().toString());
        }
        if (sn.contains("rules")) {
            c.snapshotRules.clear();
            for (const QJsonValue& v : sn.value("rules").toArray()) {
                const QJsonObject o = v.toObject();
                SnapshotRule r;
                r.root = o.value("root").toString();
                r.name = o.value("name").toString();
                r.policy = o.value("policy").toString("backup");
                r.reason = o.value("reason").toString();
                c.snapshotRules << r;
            }
        }
    }

    return c;
}

} // namespace dm
