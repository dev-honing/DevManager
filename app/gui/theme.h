#pragma once
#include <QFont>
class QApplication;

namespace dm {

// Centralized theme tokens. Widgets and the stylesheet both read from here.
namespace Color {
inline constexpr const char* Bg = "#1e1f22";
inline constexpr const char* Panel = "#25272b";
inline constexpr const char* PanelHover = "#2b2e33";
inline constexpr const char* PanelAlt = "#202225";
inline constexpr const char* Border = "#36393f";
inline constexpr const char* Primary = "#3b82f6";
inline constexpr const char* PrimaryHover = "#4b8ef7";
inline constexpr const char* TextPrimary = "#f3f4f6";
inline constexpr const char* TextSecondary = "#9ca3af";
inline constexpr const char* Muted = "#6b7280";
inline constexpr const char* Success = "#22c55e";
inline constexpr const char* Warning = "#f59e0b";
inline constexpr const char* Danger = "#ef4444";
inline constexpr const char* Purple = "#a855f7";
inline constexpr const char* Amber = "#f59e0b";
} // namespace Color

namespace Metric {
inline constexpr int OuterMargin = 18;
inline constexpr int Gap = 12;
inline constexpr int RowHeight = 32;
inline constexpr int CardPadding = 16;
inline constexpr int CardRadius = 11;
inline constexpr int SidebarWidth = 202;
inline constexpr int RightPanelWidth = 284;
} // namespace Metric

// Fusion base + dark palette + stylesheet built from the tokens above.
void applyModernTheme(QApplication& app);

// Kept for existing callers.
const char* accentHex();

// Segoe UI Variable (Win11) with a Segoe UI fallback; `display` = large optical size.
QFont uiFont(int pointSize = 10, int weight = QFont::Normal, bool display = false);

// Cascadia Mono / Consolas for paths, hashes, versions, env values.
QFont monoFont(int pointSize = 9);

} // namespace dm
