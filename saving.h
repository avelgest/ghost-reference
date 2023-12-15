#pragma once

class QJsonDocument;
class QString;

#include <QtCore/QString>

namespace sessionSaving
{

    QJsonDocument sessionToJson();
    bool saveSession(const QString &filepath);

    bool loadSession(const QString &filepath);

    QString showSaveAsDialog(const QString &directory = {});
    QString showOpenDialog(const QString &directory = {});

} // namespace sessionSaving
