#pragma once
//
// P11.2/11.3 -- generate a per-project dev container setup from
// config/project-types.json: a docker-compose.yml (named volumes for
// workspace / .claude / .codex / deps) and a .devcontainer/devcontainer.json
// so "Reopen in Container" works. Pure generation -- writes nothing, runs
// nothing; the CLI does the file I/O and `docker compose` calls.
//
#include <QString>
#include <QStringList>

namespace dm {

struct ProjectSpec {
    QString name;
    QString type;          // "cpp" | "nextjs" | ... (key in project-types.json)
    QString image;         // e.g. "ai-dev-cpp:0.1"
    QString workspace;     // in-container path, e.g. "/workspace"
    QString containerHome; // e.g. "/home/node"
    QStringList volumes;   // subset of {workspace, claude, codex, deps}
    bool docker = true;
    QString hostProfile;   // set when docker == false (e.g. "msvc-qt6")
    bool valid = false;
    QString error;
};

class ProjectEnv {
public:
    // Resolve <type> against config/project-types.json (falls back to defaults).
    static ProjectSpec resolve(const QString& name, const QString& type);

    static QString composeYaml(const ProjectSpec& s);
    static QString devcontainerJson(const ProjectSpec& s);
};

} // namespace dm
