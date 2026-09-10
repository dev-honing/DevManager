#pragma once
#include <QJsonObject>
#include <QString>

namespace dm::json {

// Writes indented UTF-8 *without* a BOM (v4.1 PowerShell wrote UTF-8+BOM,
// which broke non-Qt parsers). Returns false and sets *error on failure.
bool write(const QString& path, const QJsonObject& obj, QString* error = nullptr);

// Reads an object; tolerates a leading UTF-8 BOM. Returns {} on failure.
QJsonObject read(const QString& path, QString* error = nullptr);

} // namespace dm::json
