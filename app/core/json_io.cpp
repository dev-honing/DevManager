#include "json_io.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace dm::json {

bool write(const QString& path, const QJsonObject& obj, QString* error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (f.write(data) != data.size()) {
        if (error) *error = f.errorString();
        return false;
    }
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

QJsonObject read(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return {};
    }
    QByteArray data = f.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) *error = pe.errorString();
        return {};
    }
    return doc.object();
}

} // namespace dm::json
