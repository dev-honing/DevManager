#include "service/service_probe.h"

#include "process_runner.h"

#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>

namespace dm {

static bool tcpOpen(quint16 port, int timeoutMs = 500)
{
    QTcpSocket s;
    s.connectToHost(QStringLiteral("127.0.0.1"), port);
    return s.waitForConnected(timeoutMs);
}

QList<ServiceState> ServiceProbe::probeAll()
{
    QList<ServiceState> out;

    {
        ServiceState s{"headroom", "Headroom", ServiceState::Stopped, "port 8787"};
        if (tcpOpen(8787))
            s.level = ServiceState::Running;
        out << s;
    }
    {
        ServiceState s{"omniroute", "OmniRoute", ServiceState::Stopped, "port 20128"};
        if (tcpOpen(20128))
            s.level = ServiceState::Running;
        out << s;
    }
    {
        ServiceState s{"docker", "Docker", ServiceState::Stopped, {}};
        const QString exe = QStandardPaths::findExecutable("docker");
        if (exe.isEmpty()) {
            s.level = ServiceState::Unknown;
            s.detail = "not installed";
        } else {
            const ProcessResult r = ProcessRunner::run(
                exe, {"version", "--format", "{{.Server.Version}}"}, 4000);
            if (r.ok() && !r.firstLine().isEmpty()) {
                s.level = ServiceState::Running;
                s.detail = "engine " + r.firstLine();
            } else {
                s.detail = "engine not responding";
            }
        }
        out << s;
    }
    {
        ServiceState s{"wsl", "WSL", ServiceState::Stopped, {}};
        const QString exe = QStandardPaths::findExecutable("wsl");
        if (exe.isEmpty()) {
            s.level = ServiceState::Unknown;
            s.detail = "not installed";
        } else {
            const ProcessResult r = ProcessRunner::run(exe, {"-l", "-q", "--running"}, 4000);
            const QString text = QString::fromUtf16(
                reinterpret_cast<const char16_t*>(r.out.constData()), r.out.size() / 2);
            const QStringList running =
                text.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
            if (!running.isEmpty()) {
                s.level = ServiceState::Running;
                s.detail = running.join(", ");
            } else {
                s.detail = "no distro running";
            }
        }
        out << s;
    }

    return out;
}

} // namespace dm
