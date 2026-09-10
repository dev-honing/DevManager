#pragma once
#include <QString>
#include <QStringList>
#include "model.h"

namespace dm::path {

QString homeDir();                       // %USERPROFILE% (or QDir::homePath fallback)
QString normalizeLower(const QString& p);

// Reparse-point inspection via QFileInfo (Qt distinguishes junction vs symlink).
LinkInfo probeLink(const QString& path);

// Ports Get-DiscoveryRoots: explicit ~/.claude|.codex|.agents|.gemini[/config]|
// .config|.local /<leaf>, then <leaf> and config/<leaf> under every child of
// %USERPROFILE% / %APPDATA% / %LOCALAPPDATA%. De-duplicated, sorted.
QStringList discoveryRoots(const QString& leaf);

// Ports Get-HostFromRoot: ".claude" etc. from the path, else parent dir name.
QString hostFromRoot(const QString& rootPath);

} // namespace dm::path
