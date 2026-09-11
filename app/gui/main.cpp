#include <QApplication>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include "gui/main_window.h"
#include "gui/theme.h"

// dev: auto-accept any modal QMessageBox / dialog (for headless --shot runs)
static void acceptModals()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* mb = qobject_cast<QMessageBox*>(w)) {
            if (auto* b = mb->button(QMessageBox::Yes)) b->click();
            else if (auto* ok = mb->button(QMessageBox::Ok)) ok->click();
        }
    }
}

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
        const bool create = args.contains("--shot-create");  // click a "Create ..." button
        const int ci = args.indexOf("--shot-click");   // dev: click any button by visible text
        const QString clickText = (ci >= 0 && ci + 1 < args.size()) ? args.at(ci + 1) : QString();
        QTimer::singleShot(8000, &w, [&w, path, nav, dry, create, clickText] {
            if (!nav.isEmpty())
                w.selectPage(nav);
            if (dry) {
                for (auto* b : w.findChildren<QPushButton*>("dryCheckBtn"))
                    if (b->isVisible() && b->isEnabled())
                        b->click();
            }
            if (!clickText.isEmpty()) {
                for (auto* b : w.findChildren<QPushButton*>())
                    if (b->isVisible() && b->isEnabled() && b->text().contains(clickText))
                        b->click();
            }
            if (create) {
                // dry check runs async; give it a moment, then click Create and
                // auto-accept the confirmation dialog.
                QTimer::singleShot(6000, &w, [&w] {
                    for (auto* b : w.findChildren<QPushButton*>())
                        if (b->isVisible() && b->isEnabled()
                            && b->text().contains("Create Snapshot"))
                            b->click();
                });
                auto* poll = new QTimer(&w);
                poll->start(300);
                QObject::connect(poll, &QTimer::timeout, acceptModals);
            }
            const int wait = create ? 30000 : (dry || !clickText.isEmpty()) ? 12000 : 400;
            QTimer::singleShot(wait, &w, [&w, path] {
                w.grab().save(path);
                qApp->quit();
            });
        });
    }

    return app.exec();
}
