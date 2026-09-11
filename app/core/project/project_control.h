#pragma once
//
// The file I/O and `docker compose` calls that ProjectEnv's pure generation
// needs a caller for -- shared by the CLI (--project-init/-up/-down) and the
// GUI Projects page so the two don't duplicate the compose invocation.
//
#include <QString>

#include "project/project_env.h"

namespace dm {

struct ProjectWriteResult {
    bool ok = false;
    QString error;
    QString composePath;   // <projectDir>/.devmanager/docker-compose.yml
};

struct ComposeResult {
    bool ok = false;
    QString output;   // combined stdout+stderr
};

class ProjectControl {
public:
    // Writes .devmanager/docker-compose.yml + .devcontainer/devcontainer.json
    // under projectDir. spec.docker must be true (host-profile types have
    // nothing to write here).
    static ProjectWriteResult writeFiles(const ProjectSpec& spec, const QString& projectDir);

    static bool hasCompose(const QString& projectDir);

    // `docker compose -f <projectDir>/.devmanager/docker-compose.yml up -d`
    // and `... down`. Blocking -- call off the UI thread.
    static ComposeResult up(const QString& projectDir);
    static ComposeResult down(const QString& projectDir);
};

} // namespace dm
