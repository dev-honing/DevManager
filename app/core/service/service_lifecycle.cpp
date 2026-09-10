#include "service/service_lifecycle.h"

#include "process_runner.h"

#include <QProcess>
#include <QStandardPaths>

namespace dm {

QString lifecycleOpName(LifecycleOp op)
{
    switch (op) {
    case LifecycleOp::Status: return "status";
    case LifecycleOp::Stop: return "stop";
    case LifecycleOp::Start: return "start";
    case LifecycleOp::Restart: return "restart";
    }
    return "status";
}

static const QStringList& argsFor(const ServiceLifecycleSpec& lc, LifecycleOp op)
{
    switch (op) {
    case LifecycleOp::Status: return lc.statusArgs;
    case LifecycleOp::Stop: return lc.stopArgs;
    case LifecycleOp::Start: return lc.startArgs;
    case LifecycleOp::Restart: return lc.restartArgs;
    }
    return lc.statusArgs;
}

LifecycleResult ServiceLifecycle::run(const ServiceSpec& spec, LifecycleOp op)
{
    LifecycleResult r;
    r.serviceId = spec.id;
    r.op = op;

    const ServiceLifecycleSpec& lc = spec.lifecycle;
    if (lc.exe.isEmpty()) {
        r.error = "no lifecycle configured for " + spec.id;
        return r;
    }
    const QStringList args = argsFor(lc, op);
    if (args.isEmpty()) {
        r.error = lifecycleOpName(op) + " not configured for " + spec.id;
        return r;
    }

    QString exe = QStandardPaths::findExecutable(lc.exe);
    if (exe.isEmpty())
        exe = lc.exe;

    if (op == LifecycleOp::Start && lc.startDetached) {
        const bool started = QProcess::startDetached(exe, args);
        r.ok = started;
        r.output = started ? "started (detached)" : QString();
        if (!started)
            r.error = "could not launch " + exe;
        return r;
    }

    const ProcessResult pr = ProcessRunner::run(exe, args, 30000);
    r.ok = pr.ok();
    r.output = pr.outText();
    if (!pr.started) {
        r.error = "could not launch " + exe;
    } else if (pr.timedOut) {
        r.error = "timed out";
    } else if (!pr.ok()) {
        // the reason is often on stderr (e.g. "not supported for this
        // deployment type") -- surface it, not just the exit code
        const QString errText = QString::fromLocal8Bit(pr.err).trimmed();
        if (!errText.isEmpty())
            r.error = errText;
        else if (!pr.outText().isEmpty())
            r.error = pr.outText();
        else
            r.error = QString("exit %1").arg(pr.exitCode);
    }
    return r;
}

LifecycleResult ServiceLifecycle::run(const QString& serviceId, LifecycleOp op)
{
    const ScanConfig cfg = ScanConfig::load();
    for (const ServiceSpec& s : cfg.services)
        if (s.id == serviceId)
            return run(s, op);
    LifecycleResult r;
    r.serviceId = serviceId;
    r.op = op;
    r.error = "unknown service: " + serviceId;
    return r;
}

QStringList ServiceLifecycle::stopRunning(const QList<QString>& runningIds)
{
    const ScanConfig cfg = ScanConfig::load();
    QStringList stopped;
    for (const ServiceSpec& s : cfg.services) {
        if (!runningIds.contains(s.id) || !s.lifecycle.canControl())
            continue;
        if (run(s, LifecycleOp::Stop).ok)
            stopped << s.id;
    }
    return stopped;
}

} // namespace dm
