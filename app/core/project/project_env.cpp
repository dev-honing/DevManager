#include "project/project_env.h"

#include "json_io.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace dm {

namespace {

QString findProjectTypes()
{
    const QString ov = QProcessEnvironment::systemEnvironment().value("DEVMANAGER_CONFIG_DIR");
    if (!ov.isEmpty()) {
        const QString p = QDir(ov).filePath("project-types.json");
        if (QFileInfo::exists(p))
            return p;
    }
    for (const QString& start : {QDir::currentPath(), QCoreApplication::applicationDirPath()}) {
        QDir d(start);
        for (int i = 0; i < 6; ++i) {
            const QString p = d.filePath("config/project-types.json");
            if (QFileInfo::exists(p))
                return QDir(p).absolutePath();
            if (!d.cdUp())
                break;
        }
    }
    return {};
}

QString defaultImage(const QString& type)
{
    if (type == "cpp") return "ai-dev-cpp:0.1";
    if (type == "nextjs" || type == "next") return "ai-dev-next:0.1";
    return "ai-dev-base:0.1";
}

} // namespace

ProjectSpec ProjectEnv::resolve(const QString& name, const QString& type)
{
    ProjectSpec s;
    s.name = name;
    s.type = type;
    s.workspace = "/workspace";
    s.containerHome = "/home/node";
    s.volumes = {"workspace", "claude", "codex", "deps"};
    s.image = defaultImage(type);

    if (name.isEmpty()) {
        s.error = "project name is required";
        return s;
    }

    const QString file = findProjectTypes();
    if (!file.isEmpty()) {
        const QJsonObject root = json::read(file);
        const QJsonObject t = root.value(type).toObject();
        if (!t.isEmpty()) {
            s.docker = t.value("docker").toBool(true);
            s.hostProfile = t.value("hostProfile").toString();
            if (t.contains("image")) s.image = t.value("image").toString();
            if (t.contains("workspace")) s.workspace = t.value("workspace").toString();
            if (t.contains("containerHome")) s.containerHome = t.value("containerHome").toString();
            if (t.contains("volumes")) {
                s.volumes.clear();
                for (const QJsonValue& v : t.value("volumes").toArray())
                    s.volumes << v.toString();
            }
        }
    }

    if (!s.docker && s.hostProfile.isEmpty())
        s.hostProfile = "host";
    s.valid = true;
    return s;
}

QString ProjectEnv::composeYaml(const ProjectSpec& s)
{
    if (!s.valid || !s.docker)
        return {};

    QStringList mounts, vols;
    // the compose file lives in <project>/.devmanager/, so ".." is the project
    if (s.volumes.contains("workspace"))
        mounts << QString("      - ..:%1").arg(s.workspace);
    // compose already namespaces volumes by the project `name:`, so the bare
    // key is enough (becomes <name>_<key> on disk).
    const auto named = [&](const QString& key, const QString& target) {
        if (!s.volumes.contains(key))
            return;
        mounts << QString("      - %1:%2").arg(key, target);
        vols << QString("  %1:").arg(key);
    };
    named("claude", s.containerHome + "/.claude");
    named("codex", s.containerHome + "/.codex");
    named("deps", s.workspace + "/.deps");

    QString y;
    y += "name: " + s.name + "\n";
    y += "services:\n";
    y += "  dev:\n";
    y += "    image: " + s.image + "\n";
    y += "    command: sleep infinity\n";
    y += "    working_dir: " + s.workspace + "\n";
    y += "    volumes:\n" + mounts.join("\n") + "\n";
    y += "    extra_hosts:\n";
    y += "      - \"host.docker.internal:host-gateway\"\n";
    if (!vols.isEmpty())
        y += "volumes:\n" + vols.join("\n") + "\n";
    return y;
}

QString ProjectEnv::devcontainerJson(const ProjectSpec& s)
{
    if (!s.valid || !s.docker)
        return {};
    QJsonObject o{
        {"name", s.name},
        {"dockerComposeFile", "../.devmanager/docker-compose.yml"},
        {"service", "dev"},
        {"workspaceFolder", s.workspace},
    };
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

} // namespace dm
