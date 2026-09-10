#pragma once
#include <QList>
#include <QString>

namespace dm {

// Read-only liveness probe. Does NOT start/stop/configure anything.
struct ServiceState {
    enum Level { Unknown, Running, Stopped, Warning, Error };
    QString id;       // "headroom" | "omniroute" | "docker" | "wsl"
    QString name;     // display
    Level level = Unknown;
    QString detail;   // short human note
};

class ServiceProbe {
public:
    // Blocking (~1s worst case). Call off the GUI thread.
    static QList<ServiceState> probeAll();
};

} // namespace dm
