//
// devmanager-scan  --  Phase 1 read-only environment scan.
//
// Writes a single inventory JSON. Touches nothing else: no config dirs,
// no service control, no git mutation.
//
#include "json_io.h"
#include "scan/env_scanner.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("devmanager-scan");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Read-only scan of the local AI dev environment -> inventory JSON.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption outOpt({"o", "out"}, "Output JSON path.", "path",
                             "inventory.cpp.json");
    QCommandLineOption rootOpt("root", "Project root recorded in the inventory.",
                              "path");
    parser.addOption(outOpt);
    parser.addOption(rootOpt);
    parser.process(app);

    if (parser.isSet(rootOpt))
        QDir::setCurrent(parser.value(rootOpt));

    QTextStream err(stderr);
    err << "scanning environment (read-only)...\n";

    const dm::EnvironmentInventory inv = dm::EnvironmentScanner::scan();

    QString writeErr;
    const QString out = parser.value(outOpt);
    if (!dm::json::write(out, inv.toJson(), &writeErr)) {
        err << "error: could not write " << out << ": " << writeErr << "\n";
        return 1;
    }

    err << "wrote " << out << "\n"
        << "  tools    : " << inv.tools.size() << "\n"
        << "  skills   : " << inv.skills.size() << "\n"
        << "  plugins  : " << inv.plugins.size() << "\n"
        << "  packages : " << inv.globalPackages.size() << "\n"
        << "  env vars : " << inv.env.size() << "\n";
    return 0;
}
