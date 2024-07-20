#include "saving.h"

#include <limits>

#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QMimeData>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QTemporaryFile>

#include <QtGui/QDropEvent>
#include <QtGui/QPixmap>

#include <QtWidgets/QFileDialog>

#include "app.h"
#include "reference_image.h"
#include "utils/zip_file.h"
#include "widgets/main_toolbar.h"
#include "widgets/reference_window.h"

namespace
{

    const char *const sessionJsonName = "session.json";

    bool deleteFile(QFile &file)
    {
        const QFileInfo fileInfo(file);

        if (fileInfo.isFile())
        {
            return file.remove();
        }
        return false;
    }

    const QString &defaultSaveDirectory()
    {
        static const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return dir;
    }

    QByteArray createSessionZip()
    {
        const App *app = App::ghostRefInstance();
        const QJsonDocument json = sessionSaving::sessionToJson();

        utils::ZipFile zipFile;
        zipFile.addFile(sessionJsonName, json.toJson());

        for (const auto &refWindow : app->referenceWindows())
        {
            for (const auto &refItem : refWindow->referenceImages())
            {
                QByteArray itemData = refItem->ensureCompressedImage();
                // FIXME Ensure name is unique
                zipFile.addFile(refItem->name(), std::move(itemData));
            }
        }

        return zipFile.toBuffer();
    }

    bool loadReferenceItems(const QJsonDocument &doc,
                            const utils::ZipFile &zipFile,
                            QList<ReferenceImageSP> &newItemsOut)
    {
        const QJsonObject docObj = doc.object();

        QJsonObject references;
        if (const QJsonValue val = docObj["references"]; val.isObject())
        {
            references = val.toObject();
        }
        else
        {
            qCritical() << "Session file is invalid: missing 'references' JSON object.";
            return false;
        }

        QMap<QString, QPixmap> pixmapMap;

        for (const auto &refName : references.keys())
        {
            const QByteArray &imgData = zipFile.getFile(refName);

            if (!imgData.isEmpty())
            {
                QPixmap pixmap;
                pixmap.loadFromData(imgData);
                if (pixmap.isNull())
                {
                    qWarning() << "Unable to load embeded reference " << refName;
                    continue;
                }
                pixmapMap.insert(refName, pixmap);
            }
        }

        App *app = App::ghostRefInstance();
        newItemsOut.append(std::move(app->referenceItems().loadJson(references, pixmapMap)));
        return true;
    }

    bool loadReferenceWindows(const QJsonDocument &doc)
    {
        const QJsonObject docObj = doc.object();

        QJsonArray refWindows;
        if (const QJsonValue val = docObj["windows"]; val.isArray())
        {
            refWindows = val.toArray();
        }
        else
        {
            qCritical() << "Session file is invalid: missing 'windows' JSON array.";
            return false;
        }

        App *app = App::ghostRefInstance();
        app->closeAllReferenceWindows();

        for (auto windowJson : refWindows)
        {
            if (!windowJson.isObject())
            {
                qWarning() << "Invalid value in" << sessionJsonName << "windows array";
                continue;
            }
            ReferenceWindow *newWindow = app->newReferenceWindow();
            newWindow->fromJson(windowJson.toObject());
            newWindow->show();
        }

        return true;
    }

    bool loadToolbarPos(const QJsonDocument &doc)
    {
        const QJsonObject docObj = doc.object();
        if (const QJsonValue val = docObj["toolbarPos"]; val.isArray())
        {
            const QJsonArray valArray = val.toArray();
            const int x = static_cast<int>(valArray.at(0).toInteger());
            const int y = static_cast<int>(valArray.at(1).toInteger());

            App *app = App::ghostRefInstance();
            app->mainToolbar()->move({x, y});
            return true;
        }

        // TODO Warn if values are invalid
        return false;
    }

    bool loadSessionFromZip(QByteArray &zipBuffer)
    {
        const utils::ZipFile zipFile = utils::ZipFile::fromBuffer(zipBuffer);

        const QByteArray &sessionJson = zipFile.getFile(sessionJsonName);

        if (sessionJson.isEmpty())
        {
            qCritical() << "Session file is invalid:" << sessionJsonName << " is empty ";
            return false;
        }

        QJsonParseError jsonErr;
        const QJsonDocument jsonDoc = QJsonDocument::fromJson(sessionJson, &jsonErr);

        if (jsonErr.error != QJsonParseError::NoError)
        {
            qCritical() << "Json error loading session:" << jsonErr.errorString();
            return false;
        }

        if (!jsonDoc.isObject())
        {
            qCritical() << "Session file is invalid:" << sessionJsonName
                        << "does not have an object at the top level";
            return false;
        }

        loadToolbarPos(jsonDoc);

        // Need to keep the shared pointers to prevent the ReferenceImages from being destroyed
        QList<ReferenceImageSP> refItems;
        if (!loadReferenceItems(jsonDoc, zipFile, refItems))
        {
            return false;
        }

        return loadReferenceWindows(jsonDoc);
    }

} // namespace

namespace sessionSaving
{
    QJsonDocument sessionToJson()
    {
        const App *app = App::ghostRefInstance();
        QJsonObject json;

        QJsonArray jsonWindows;
        for (const auto &window : app->referenceWindows())
        {
            jsonWindows.append(window->toJson());
        }
        json["windows"] = jsonWindows;

        json["references"] = app->referenceItems().toJson();

        const QPoint toolbarPos = app->mainToolbar()->pos();
        json["toolbarPos"] = QJsonArray({toolbarPos.x(), toolbarPos.y()});

        return QJsonDocument(json);
    }

    bool saveSession(const QString &filepath)
    {
        const QByteArray sessionZip = createSessionZip();
        if (sessionZip.isEmpty())
        {
            qCritical() << "Error creating zip from session.";
            return false;
        }

        // TODO Use QSaveFile
        QTemporaryFile tempFile(filepath + ".XXXXXX.tmp");

        if (!tempFile.open())
        {
            qCritical() << "Unable to open temp file " << tempFile.fileName();
            return false;
        }

        QDataStream dataStream(&tempFile);
        dataStream.writeRawData(sessionZip.constData(), static_cast<int>(sessionZip.size()));

        QFile destFile(filepath);
        if (destFile.exists())
        {
            deleteFile(destFile);
        }

        if (!tempFile.rename(filepath))
        {
            qCritical() << "Saving session failed. Unable to rename tmp file to" << filepath;
            return false;
        }

        tempFile.setAutoRemove(false);
        qInfo() << "Session saved as" << QFileInfo(destFile).absoluteFilePath();
        return true;
    }

    bool loadSession(const QString &filepath)
    {
        QFile file(filepath);
        if (!file.open(QFile::ReadOnly))
        {
            qCritical() << "Unable to open file" << filepath;
            return false;
        }
        const QFileInfo fileInfo(file);
        if (fileInfo.size() > std::numeric_limits<int32_t>::max())
        {
            qCritical() << "Cannot load Session file" << filepath << "File is too large";
            return false;
        }

        QByteArray buf = file.readAll();

        if (!loadSessionFromZip(buf))
        {
            qCritical() << "Unable to load session from " << filepath;
            return false;
        }

        qInfo() << "Loaded session" << fileInfo.absoluteFilePath();
        return true;
    }

    QString showSaveAsDialog(const QString &directory)
    {
        return QFileDialog::getSaveFileName(nullptr, "Save Ghost Reference Session",
                                            directory.isEmpty() ? defaultSaveDirectory() : directory,
                                            "Ghost Reference Session (*.ghr)");
    }

    QString showOpenDialog(const QString &directory)
    {
        return QFileDialog::getOpenFileName(nullptr, "Open Ghost Reference Session",
                                            directory.isEmpty() ? defaultSaveDirectory() : directory,
                                            "Ghost Reference Session (*.ghr)");
    }

    QString getSessionFilePath(const QDropEvent *dropEvent)
    {
        const QMimeData *mimeData = dropEvent->mimeData();
        if (mimeData->hasUrls())
        {
            for (auto &url : mimeData->urls())
            {
                if (const QString filePath = url.toLocalFile(); isSessionFile(filePath))
                {
                    return filePath;
                }
            }
        }
        if (mimeData->hasText())
        {
            return mimeData->text();
        }
        return {};
    }

    bool isSessionFile(const QDropEvent *dropEvent)
    {
        return isSessionFile(getSessionFilePath(dropEvent));
    }

    bool isSessionFile(const QString &path)
    {
        return path.toLower().endsWith(".ghr");
    }

} // namespace sessionSaving
