#pragma once
class QApplication;

namespace dm {

// Fusion base + a modern dark palette and stylesheet. One call, no deps.
void applyModernTheme(QApplication& app);

// Accent color as a hex string, for per-widget use (e.g. highlighting rows).
const char* accentHex();

} // namespace dm
