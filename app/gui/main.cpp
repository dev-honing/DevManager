#include <QApplication>
#include <QPushButton>
#include <QTimer>

#include "gui/main_window.h"
#include "gui/theme.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("DevManager");
    QApplication::setOrganizationName("DevManager");

    dm::applyModernTheme(app);

    const QStringList args = app.arguments();

    dm::MainWindow w;
    const int wi = args.indexOf("--shot-size");   // dev: "--shot-size 980x700"
    if (wi >= 0 && wi + 1 < args.size()) {
        const QStringList wh = args.at(wi + 1).split('x');
        if (wh.size() == 2)
            w.resize(wh[0].toInt(), wh[1].toInt());
    }
    w.show();

    // dev affordance:  --shot <png>  renders one frame after the first scan and exits
    const int si = args.indexOf("--shot");
    if (si >= 0 && si + 1 < args.size()) {
        const QString path = args.at(si + 1);
        const int ni = args.indexOf("--shot-nav");
        const QString nav = (ni >= 0 && ni + 1 < args.size()) ? args.at(ni + 1) : QString();
        const bool dry = args.contains("--shot-dryrun");
        QTimer::singleShot(8000, &w, [&w, path, nav, dry] {
            if (!nav.isEmpty())
                w.selectPage(nav);
            if (dry) {
                for (auto* b : w.findChildren<QPushButton*>("dryCheckBtn"))
                    if (b->isVisible() && b->isEnabled())
                        b->click();
            }
            QTimer::singleShot(dry ? 12000 : 400, &w, [&w, path] {
                w.grab().save(path);
                qApp->quit();
            });
        });
    }

    return app.exec();
}
