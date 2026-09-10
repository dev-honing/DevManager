#include <QApplication>
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
        QTimer::singleShot(8000, &w, [&w, path, nav] {
            if (!nav.isEmpty())
                w.selectPage(nav);
            QTimer::singleShot(400, &w, [&w, path] {
                w.grab().save(path);
                qApp->quit();
            });
        });
    }

    return app.exec();
}
