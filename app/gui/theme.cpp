#include "gui/theme.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace dm {

namespace {
constexpr const char* kAccent = "#3b82f6";
constexpr const char* kBg = "#1e1f22";
constexpr const char* kPanel = "#2b2d30";
constexpr const char* kPanel2 = "#26282b";
constexpr const char* kBorder = "#393b40";
constexpr const char* kText = "#dfe1e5";
constexpr const char* kMuted = "#9aa0a6";
} // namespace

const char* accentHex() { return kAccent; }

void applyModernTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QFont f = app.font();
#ifdef Q_OS_WIN
    f.setFamily("Segoe UI");
#endif
    f.setPointSize(10);
    app.setFont(f);

    QPalette p;
    p.setColor(QPalette::Window, QColor(kBg));
    p.setColor(QPalette::WindowText, QColor(kText));
    p.setColor(QPalette::Base, QColor(kPanel));
    p.setColor(QPalette::AlternateBase, QColor(kPanel2));
    p.setColor(QPalette::Text, QColor(kText));
    p.setColor(QPalette::Button, QColor(kPanel));
    p.setColor(QPalette::ButtonText, QColor(kText));
    p.setColor(QPalette::ToolTipBase, QColor(kPanel));
    p.setColor(QPalette::ToolTipText, QColor(kText));
    p.setColor(QPalette::Highlight, QColor(kAccent));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::PlaceholderText, QColor(kMuted));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(kMuted));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(kMuted));
    p.setColor(QPalette::Link, QColor(kAccent));
    app.setPalette(p);

    app.setStyleSheet(QString(R"(
        QWidget { color: %(text); }
        QMainWindow, QDialog { background: %(bg); }

        #header { background: %(bg); border-bottom: 1px solid %(border); }
        #appTitle { font-size: 16px; font-weight: 600; letter-spacing: 0.3px; }
        #headerStatus { color: %(muted); }

        QPushButton#primaryBtn {
            background: %(accent); color: #ffffff; border: none;
            border-radius: 6px; padding: 7px 16px; font-weight: 600;
        }
        QPushButton#primaryBtn:hover { background: #4b8ef7; }
        QPushButton#primaryBtn:disabled { background: %(border); color: %(muted); }

        #statCard {
            background: %(panel); border: 1px solid %(border);
            border-radius: 10px;
        }
        #statValue { font-size: 22px; font-weight: 700; }
        #statCaption { color: %(muted); font-size: 11px; text-transform: uppercase;
                       letter-spacing: 0.6px; }

        QListWidget#nav {
            background: %(bg); border: none; border-right: 1px solid %(border);
            outline: 0; padding: 8px 6px;
        }
        QListWidget#nav::item {
            padding: 9px 14px; border-radius: 7px; margin: 2px 0; color: %(muted);
        }
        QListWidget#nav::item:hover { background: %(panel2); color: %(text); }
        QListWidget#nav::item:selected {
            background: %(panel); color: %(text); font-weight: 600;
            border-left: 3px solid %(accent);
        }

        QStackedWidget > QWidget { background: %(bg); }

        QTreeWidget, QTableWidget {
            background: %(panel); alternate-background-color: %(panel2);
            border: 1px solid %(border); border-radius: 10px;
            gridline-color: transparent; outline: 0;
        }
        QTreeWidget::item, QTableWidget::item { padding: 6px 8px; }
        QTreeWidget::item:selected, QTableWidget::item:selected {
            background: %(accent); color: #ffffff;
        }
        QHeaderView::section {
            background: %(panel); color: %(muted); border: none;
            border-bottom: 1px solid %(border); padding: 8px 8px; font-weight: 600;
        }
        QTreeView::branch { background: %(panel); }

        QLineEdit {
            background: %(panel2); border: 1px solid %(border); border-radius: 7px;
            padding: 7px 10px; selection-background-color: %(accent);
        }
        QLineEdit:focus { border: 1px solid %(accent); }

        QToolBar { background: %(bg); border: none; spacing: 8px; padding: 6px 12px; }
        QStatusBar { background: %(bg); color: %(muted); border-top: 1px solid %(border); }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar::handle:vertical { background: %(border); border-radius: 5px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: #4a4d52; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
        QScrollBar::handle:horizontal { background: %(border); border-radius: 5px; min-width: 24px; }
    )")
                           .replace("%(accent)", kAccent)
                           .replace("%(bg)", kBg)
                           .replace("%(panel2)", kPanel2)
                           .replace("%(panel)", kPanel)
                           .replace("%(border)", kBorder)
                           .replace("%(muted)", kMuted)
                           .replace("%(text)", kText));
}

} // namespace dm
