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

    dm::MainWindow w;
    w.show();

    // dev affordance:  --shot <png>  renders one frame after the first scan and exits
    const QStringList args = app.arguments();
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
                bool clicked = false;
                for (auto* b : w.findChildren<QPushButton*>("dryCheckBtn"))
                    if (b->isVisible() && b->isEnabled()) {
                        b->click();
                        clicked = true;
                    }
                fprintf(stderr, "shot-dryrun: clicked=%d\n", clicked);
            }
            QTimer::singleShot(dry ? 12000 : 400, &w, [&w, path] {
                w.grab().save(path);
                qApp->quit();
            });
        });
    }

    return app.exec();
}
