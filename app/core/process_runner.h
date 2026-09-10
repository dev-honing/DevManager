#pragma once
//
// Thin synchronous QProcess wrapper. External CLIs (git, npm, pip, wsl,
// vswhere, headroom) are the authoritative source for the facts they own;
// this is the only sanctioned way to shell out.
//
#include <QString>
#include <QStringList>
#include <QByteArray>

namespace dm {

struct ProcessResult {
    bool started = false;   // false => program not found / failed to launch
    bool timedOut = false;
    int exitCode = -1;
    QByteArray out;
    QByteArray err;

    bool ok() const { return started && !timedOut && exitCode == 0; }
    QString outText() const { return QString::fromLocal8Bit(out).trimmed(); }
    QString firstLine() const;
};

class ProcessRunner {
public:
    // timeoutMs <= 0 waits indefinitely (avoid; callers pass a real bound).
    static ProcessResult run(const QString& program,
                             const QStringList& args = {},
                             int timeoutMs = 8000);
};

} // namespace dm
