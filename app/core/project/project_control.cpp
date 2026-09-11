#include "project/project_control.h"

#include "json_io.h"
#include "process_runner.h"

#include <QFileInfo>

namespace dm {

namespace {
QString composePathFor(const QString& projectDir)
{
    return projectDir + "/.devmanager/docker-compose.yml";
}
} // namespace

ProjectWriteResult ProjectControl::writeFiles(const ProjectSpec& spec,
                                              const QString& projectDir)
{
    ProjectWriteResult res;
    if (!spec.valid) {
        res.error = spec.error.isEmpty() ? "invalid project spec" : spec.error;
        return res;
    }
    if (!spec.docker) {
        res.error = QString("'%1' is a host profile (%2) -- no container to write")
                        .arg(spec.type, spec.hostProfile);
        return res;
    }
    res.composePath = composePathFor(projectDir);
    QString e;
    if (!json::writeText(res.composePath, ProjectEnv::composeYaml(spec), &e)
        || !json::writeText(projectDir + "/.devcontainer/devcontainer.json",
                            ProjectEnv::devcontainerJson(spec), &e)) {
        res.error = e;
        return res;
    }
    res.ok = true;
    return res;
}

bool ProjectControl::hasCompose(const QString& projectDir)
{
    return QFileInfo::exists(composePathFor(projectDir));
}

ComposeResult ProjectControl::up(const QString& projectDir)
{
    const ProcessResult r = ProcessRunner::run(
        "docker", {"compose", "-f", composePathFor(projectDir), "up", "-d"}, 120000);
    return {r.ok(), QString::fromLocal8Bit(r.out) + QString::fromLocal8Bit(r.err)};
}

ComposeResult ProjectControl::down(const QString& projectDir)
{
    const ProcessResult r = ProcessRunner::run(
        "docker", {"compose", "-f", composePathFor(projectDir), "down"}, 120000);
    return {r.ok(), QString::fromLocal8Bit(r.out) + QString::fromLocal8Bit(r.err)};
}

} // namespace dm
