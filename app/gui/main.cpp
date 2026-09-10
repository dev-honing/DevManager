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
        QTimer::singleShot(9000, &w, [&w, path] {
            w.grab().save(path);
            qApp->quit();
        });
    }

    return app.exec();
}
