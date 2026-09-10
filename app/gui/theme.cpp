#include "gui/theme.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

namespace dm {

static QString pickFamily(std::initializer_list<QString> candidates,
                          const QString& fallback)
{
    const auto have = QFontDatabase::families();
    for (const QString& c : candidates)
        if (have.contains(c, Qt::CaseInsensitive))
            return c;
    return fallback;
}

QFont uiFont(int pointSize, int weight, bool display)
{
    static const QString textFam =
        pickFamily({"Segoe UI Variable Text", "Segoe UI Variable"}, "Segoe UI");
    static const QString dispFam =
        pickFamily({"Segoe UI Variable Display", "Segoe UI Variable"}, "Segoe UI");
    QFont f(display ? dispFam : textFam, pointSize, weight);
    f.setStyleStrategy(QFont::PreferAntialias);
    if (display)
        f.setLetterSpacing(QFont::AbsoluteSpacing, -0.3);
    return f;
}

QFont monoFont(int pointSize)
{
    static const QString fam =
        pickFamily({"Cascadia Mono", "Cascadia Code", "JetBrains Mono"}, "Consolas");
    QFont f(fam, pointSize);
    f.setStyleHint(QFont::Monospace);
    return f;
}

const char* accentHex() { return Color::Primary; }

static QString styleSheet()
{
    // sections kept separate for readability; %(token) placeholders resolved below
    const QString sheet = QStringLiteral(R"QSS(
/* base */
QWidget { color: %(text); }
QMainWindow, QDialog { background: %(bg); }
QToolTip { background: %(panel); color: %(text); border: 1px solid %(border);
           padding: 4px 7px; border-radius: 5px; }

/* top bar */
#topBar { background: %(bg); border-bottom: 1px solid %(border); }
#appLogo { background: %(primary); border-radius: 7px; color: #ffffff;
           font-weight: 800; }
#appTitle { font-size: 15px; font-weight: 700; }
#appSubtitle { color: %(muted); font-size: 11px; }
#topSearch { background: %(panelAlt); border: 1px solid %(border);
             border-radius: 8px; padding: 7px 10px; }
#topSearch:focus { border: 1px solid %(primary); }
#lastScanned { color: %(muted); font-size: 12px; }

QPushButton#primaryBtn, QPushButton#dryCheckBtn {
    background: %(primary); color: #ffffff; border: none;
    border-radius: 7px; padding: 8px 16px; font-weight: 600;
}
QPushButton#primaryBtn:hover, QPushButton#dryCheckBtn:hover { background: %(primaryHover); }
QPushButton#primaryBtn:disabled, QPushButton#dryCheckBtn:disabled { background: %(border); color: %(muted); }

QPushButton#secondaryBtn {
    background: %(panel); color: %(text); border: 1px solid %(border);
    border-radius: 7px; padding: 8px 14px;
}
QPushButton#secondaryBtn:hover { background: %(panelHover); border-color: %(primary); }
QPushButton#secondaryBtn:disabled { color: %(muted); }

QToolButton#iconBtn { background: transparent; border: none; border-radius: 6px;
                      padding: 5px; }
QToolButton#iconBtn:hover { background: %(panelHover); }

/* sidebar */
#sidebar { background: %(bg); border-right: 1px solid %(border); }
#navItem { background: transparent; border: none; border-radius: 8px; text-align: left;
           padding: 8px 12px; color: %(textSecondary); font-size: 13px; }
#navItem:hover { background: %(panelHover); color: %(text); }
#navItem[selected="true"] {
    background: %(panelHover); color: %(text); font-weight: 600;
    border-left: 3px solid %(primary); padding-left: 9px;
}
#navBadge { background: %(panelHover); color: %(textSecondary);
            border-radius: 8px; padding: 1px 7px; font-size: 11px; }
#navSep { color: %(muted); font-size: 10px; font-weight: 700;
          letter-spacing: 1px; padding: 12px 12px 4px 12px; }
#navFootItem { background: transparent; border: none; text-align: left;
               padding: 6px 12px; color: %(muted); font-size: 12px; }
#navFootItem:hover { color: %(text); }

/* summary cards */
#summaryCard { background: %(panel); border: 1px solid %(border); border-radius: 12px; }
#summaryCard:hover { border-color: %(primary); }
#cardValue { font-size: 24px; font-weight: 700; }
#cardLabel { color: %(muted); font-size: 10px; text-transform: uppercase;
             letter-spacing: 0.8px; }
#cardHint { color: %(textSecondary); font-size: 11px; }

/* page chrome */
#pageTitle { font-size: 17px; font-weight: 700; }
#pageSubtitle { color: %(muted); font-size: 12px; }
#panelCard { background: %(panel); border: 1px solid %(border);
             border-radius: 12px; }
#mono { }

/* segmented control */
QToolButton#segBtn { background: transparent; border: none; color: %(textSecondary);
                     padding: 6px 14px; border-radius: 6px; font-size: 12px; }
QToolButton#segBtn:hover { color: %(text); }
QToolButton#segBtn:checked { background: %(panel); color: %(text); font-weight: 600; }
#segTrack { background: %(panelAlt); border: 1px solid %(border); border-radius: 8px; }

/* tables & trees */
QTreeWidget, QTableWidget, QTableView, QTreeView {
    background: %(panel); alternate-background-color: %(panelAlt);
    border: 1px solid %(border); border-radius: 12px;
    gridline-color: transparent; outline: 0;
}
QTreeWidget::item, QTableWidget::item, QTableView::item {
    padding: 6px 8px; border: none;
}
QTreeWidget::item:selected, QTableWidget::item:selected, QTableView::item:selected {
    background: %(navSel); color: %(text);
}
QHeaderView::section {
    background: %(panel); color: %(muted); border: none;
    border-bottom: 1px solid %(border); padding: 9px 8px; font-weight: 600;
    font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px;
}
QTreeView::branch { background: %(panel); }

/* inputs, misc */
QLineEdit {
    background: %(panelAlt); border: 1px solid %(border); border-radius: 8px;
    padding: 7px 10px; selection-background-color: %(primary);
}
QLineEdit:focus { border: 1px solid %(primary); }
QStatusBar { background: %(bg); color: %(muted); border-top: 1px solid %(border); }
QStatusBar::item { border: none; }
QSplitter::handle { background: %(border); }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: %(border); border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #4a4d52; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: %(border); border-radius: 5px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
)QSS");

    QColor navSel(Color::Primary);
    navSel.setAlpha(38);

    return QString(sheet)
        .replace("%(bg)", Color::Bg)
        .replace("%(panelHover)", Color::PanelHover)
        .replace("%(panelAlt)", Color::PanelAlt)
        .replace("%(panel)", Color::Panel)
        .replace("%(border)", Color::Border)
        .replace("%(primaryHover)", Color::PrimaryHover)
        .replace("%(primary)", Color::Primary)
        .replace("%(textSecondary)", Color::TextSecondary)
        .replace("%(text)", Color::TextPrimary)
        .replace("%(muted)", Color::Muted)
        .replace("%(navSel)", QString("#2d3340"));
}

void applyModernTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(uiFont(10));

    QPalette p;
    p.setColor(QPalette::Window, QColor(Color::Bg));
    p.setColor(QPalette::WindowText, QColor(Color::TextPrimary));
    p.setColor(QPalette::Base, QColor(Color::Panel));
    p.setColor(QPalette::AlternateBase, QColor(Color::PanelAlt));
    p.setColor(QPalette::Text, QColor(Color::TextPrimary));
    p.setColor(QPalette::Button, QColor(Color::Panel));
    p.setColor(QPalette::ButtonText, QColor(Color::TextPrimary));
    p.setColor(QPalette::ToolTipBase, QColor(Color::Panel));
    p.setColor(QPalette::ToolTipText, QColor(Color::TextPrimary));
    p.setColor(QPalette::Highlight, QColor(Color::Primary));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::PlaceholderText, QColor(Color::Muted));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(Color::Muted));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(Color::Muted));
    p.setColor(QPalette::Link, QColor(Color::Primary));
    app.setPalette(p);

    app.setStyleSheet(styleSheet());
}

} // namespace dm
