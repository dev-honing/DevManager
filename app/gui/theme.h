#pragma once
#include <QFont>
class QApplication;

namespace dm {

// Fusion base + a modern dark palette and stylesheet. One call, no deps.
void applyModernTheme(QApplication& app);

// Accent color as a hex string, for per-widget use (e.g. highlighting rows).
const char* accentHex();

// Segoe UI Variable (Win11) with graceful fallback. `display` picks the
// optical size tuned for large text (titles, stat numbers).
QFont uiFont(int pointSize = 10, int weight = QFont::Normal, bool display = false);

// Cascadia Mono / Consolas for paths, hashes, versions, env values.
QFont monoFont(int pointSize = 9);

} // namespace dm
