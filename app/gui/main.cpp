#include <QApplication>

#include "gui/main_window.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("DevManager");
    QApplication::setOrganizationName("DevManager");

    dm::MainWindow w;
    w.show();
    return app.exec();
}
