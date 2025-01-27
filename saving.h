#pragma once

class QDropEvent;
class QJsonDocument;
class QString;

#include <QtCore/QString>

namespace sessionSaving
{

    QJsonDocument sessionToJson();
    bool saveSession(const QString &filepath);

    bool loadSession(const QString &filepath);

    QString showSaveAsDialog(const QString &directory = {});
    QString showOpenDialog(const QString &directory = {}, bool sessions = true, bool references = true);

    QString getSessionFilePath(const QDropEvent *dropEvent);
    bool isSessionFile(const QDropEvent *dropEvent);
    bool isSessionFile(const QString &path);

} // namespace sessionSaving
