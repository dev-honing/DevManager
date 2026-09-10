#include "process_runner.h"

#include <QProcess>

namespace dm {

QString ProcessResult::firstLine() const
{
    const QString t = QString::fromLocal8Bit(out);
    const int nl = t.indexOf('\n');
    return (nl < 0 ? t : t.left(nl)).trimmed();
}

ProcessResult ProcessRunner::run(const QString& program,
                                 const QStringList& args,
                                 int timeoutMs)
{
    ProcessResult r;
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.start();

    if (!p.waitForStarted(3000)) {
        r.started = false;
        return r;
    }
    r.started = true;

    if (!p.waitForFinished(timeoutMs > 0 ? timeoutMs : -1)) {
        r.timedOut = true;
        p.kill();
        p.waitForFinished(1000);
        r.out = p.readAllStandardOutput();
        r.err = p.readAllStandardError();
        return r;
    }

    r.exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    r.out = p.readAllStandardOutput();
    r.err = p.readAllStandardError();
    return r;
}

} // namespace dm
