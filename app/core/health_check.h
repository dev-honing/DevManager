#pragma once
//
// "Is this machine ready?" -- a read-only pass over the configured tools,
// backup roots and services, for use right after restoring onto a new PC.
// Runs the scanner + service probe, changes nothing.
//
#include <QList>
#include <QString>

namespace dm {

struct HealthItem {
    QString group;    // "tool" | "backup-root" | "service"
    QString name;
    QString status;   // "ok" | "warn" | "fail"
    QString detail;
};

struct HealthReport {
    bool ok = false;   // no "fail" items
    int okCount = 0;
    int warnCount = 0;
    int failCount = 0;
    QList<HealthItem> items;
};

class HealthCheck {
public:
    static HealthReport run();
};

} // namespace dm
