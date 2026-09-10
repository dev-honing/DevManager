#pragma once
//
// New-PC bootstrap: from a health check, work out which configured tools are
// missing and emit the install command each one's config carries
// (ToolSpec.install). plan() is read-only -- it never installs anything; the
// caller prints the commands for the user to review and run.
//
#include <QList>
#include <QString>

namespace dm {

struct BootstrapStep {
    QString tool;
    QString command;    // from ToolSpec.install; empty if none configured
    bool haveHint = false;
};

struct BootstrapPlan {
    QList<BootstrapStep> steps;   // one per missing tool
    int missing = 0;
    int withHint = 0;
};

class Bootstrap {
public:
    static BootstrapPlan plan();
};

} // namespace dm
