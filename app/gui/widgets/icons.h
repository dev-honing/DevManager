#pragma once
#include <QIcon>
#include <QPixmap>
#include <QString>
class QColor;

// Tiny QPainter-drawn monochrome icon set. No QtSvg, no asset files.
// Names: environment, skills, plugins, packages, env, snapshots, restore,
// settings, docs, about, refresh, copy, folder, search, cpu, database,
// container, wsl, dot.
namespace dm::icons {

QPixmap pixmap(const QString& name, const QColor& color, int size = 16);
QIcon icon(const QString& name, const QColor& color, int size = 16);

} // namespace dm::icons
