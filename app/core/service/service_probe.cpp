#include "service/service_probe.h"

#include "config/scan_config.h"
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
    const ScanConfig cfg = ScanConfig::load();
    QList<ServiceState> out;

    for (const ServiceSpec& spec : cfg.services) {
        ServiceState s;
        s.id = spec.id;
        s.name = spec.name;
        s.level = ServiceState::Stopped;

        if (spec.wslRunning) {
            const QString exe = QStandardPaths::findExecutable("wsl");
            if (exe.isEmpty()) {
                s.level = ServiceState::Unknown;
                s.detail = "not installed";
            } else {
                const ProcessResult r =
                    ProcessRunner::run(exe, {"-l", "-q", "--running"}, 4000);
                const QString text = QString::fromUtf16(
                    reinterpret_cast<const char16_t*>(r.out.constData()),
                    r.out.size() / 2);
                const QStringList running = text.split(
                    QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
                if (!running.isEmpty()) {
                    s.level = ServiceState::Running;
                    s.detail = running.join(", ");
                } else {
                    s.detail = "no distro running";
                }
            }
        } else if (!spec.cliCheck.isEmpty()) {
            const QString exe = QStandardPaths::findExecutable(spec.cliCheck.first());
            if (exe.isEmpty()) {
                s.level = ServiceState::Unknown;
                s.detail = "not installed";
            } else {
                const ProcessResult r =
                    ProcessRunner::run(exe, spec.cliCheck.mid(1), 4000);
                if (r.ok() && !r.firstLine().isEmpty()) {
                    s.level = ServiceState::Running;
                    s.detail = r.firstLine();
                } else {
                    s.detail = "not responding";
                }
            }
        } else if (spec.port > 0) {
            s.detail = QString("port %1").arg(spec.port);
            if (tcpOpen(static_cast<quint16>(spec.port)))
                s.level = ServiceState::Running;
        } else {
            s.level = ServiceState::Unknown;
        }

        out << s;
    }
    return out;
}

} // namespace dm
