#include "reference_loading.h"

#include <QtCore/QFileInfo>
#include <QtCore/QFuture>
#include <QtCore/QMimeData>
#include <QtCore/QMimeDatabase>

#include <QtGui/QClipboard>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QPixmap>

#include "app.h"
#include "reference_collection.h"
#include "reference_image.h"

#include "utils/network_download.h"
#include "utils/result.h"

namespace
{
    class LoadException : QException
    {
        void raise() const override { throw *this; }
        LoadException *clone() const override { return new LoadException(*this); }
    };

    struct LoadedImage
    {
        QByteArray fileData;
        QImage image;
    };

    using ImageResult = utils::result<LoadedImage, QString>;

    ImageResult loadLocalImage(const QString &filepath)
    {
        const qint64 maxFileSize = 1e9;

        if (const QImageReader imageReader(filepath); !imageReader.canRead())
        {
            qCritical() << "Unable to load " << filepath << " " << imageReader.errorString();
            return ImageResult::Err(imageReader.errorString());
        }

        if (QFile file(filepath); file.open(QIODevice::ReadOnly))
        {
            const QByteArray fileData = file.read(maxFileSize);
            if (!file.atEnd())
            {
                qCritical() << filepath << "exceeds maximum file size";
                return ImageResult::Err("Exceeded maximum file size");
            }

            if (QImage image; image.loadFromData(fileData))
            {
                return LoadedImage(fileData, image);
            }
            qCritical() << "Unable to load file " << filepath;
            return ImageResult::Err("Unable to load file");
        }

        const QString msg("Unable to open file %1");
        return ImageResult::Err(msg.arg(filepath));
    }

    ReferenceCollection &getRefCollection()
    {
        return App::ghostRefInstance()->referenceItems();
    }

    QString stripExt(const QString &filepath)
    {
        const qsizetype splitPos = filepath.lastIndexOf(".");
        if (splitPos > 0)
        {
            return filepath.first(splitPos);
        }
        return filepath;
    }

} // namespace

QList<ReferenceImageSP> refLoad::fromClipboard()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData)
    {
        qCritical() << "Could not get clipboard data";
        return {};
    }
    return fromMimeData(mimeData);
}

QList<ReferenceImageSP> refLoad::fromDropEvent(const QDropEvent *event)
{
    return fromMimeData(event->mimeData());
}

ReferenceImageSP refLoad::fromFilepath(const QString &filepath)
{
    return fromUrl(filepath);
}

ReferenceImageSP refLoad::fromImage(const QImage &image)
{
    ReferenceImageSP refImage = getRefCollection().newReferenceImage();
    refImage->setLoader(std::make_unique<RefImageLoader>(image));

    return refImage;
}

QList<ReferenceImageSP> refLoad::fromMimeData(const QMimeData *mimeData)
{
    if (mimeData->hasUrls())
    {
        QList<ReferenceImageSP> results;
        const QList<QUrl> urls = mimeData->urls();

        // When dragging and dropping images from Firefox on Windows the URL to a bitmap
        // in the %TEMP% folder may be given instead of to the image itself (QTBUG-13725).
        // Try to get around this by using the text of the mimedata instead.
        if (urls.length() == 1 && urls[0].isLocalFile())
        {
            const QString path = urls[0].toLocalFile().toLower();
            if (path.endsWith(".bmp") && path.contains("/temp/") && mimeData->hasText())
            {
                if (auto newUrl = QUrl(mimeData->text()); isSupported(newUrl))
                {
                    results.push_back(refLoad::fromUrl(newUrl));
                    return results;
                }
            }
        }

        for (const auto &url : urls)
        {
            auto result = refLoad::fromUrl(url);
            results.push_back(result);
        }
        return results;
    }
    if (mimeData->hasImage())
    {
        auto qImage = qvariant_cast<QImage>(mimeData->imageData());
        return {refLoad::fromImage(qImage)};
    }

    return {};
}

ReferenceImageSP refLoad::fromUrl(const QUrl &url)
{
    const QString name = stripExt(url.fileName());
    ReferenceImageSP refImage = getRefCollection().newReferenceImage(name);
    refImage->setFilepath(url.toLocalFile());
    refImage->setLoader(std::make_unique<RefImageLoader>(url));

    return refImage;
}

bool refLoad::isSupported(const QMimeData *mimeData)
{
    if (mimeData->hasImage())
    {
        return true;
    }
    if (mimeData->hasUrls())
    {
        for (const auto &url : mimeData->urls())
        {
            if (isSupported(url))
            {
                return true;
            }
        }
    }
    return false;
}

bool refLoad::isSupported(const QDropEvent *event)
{
    return isSupported(event->mimeData());
}

bool refLoad::isSupported(const QUrl &url)
{
    static const QList<QByteArray> supportedMimeTypes = QImageReader::supportedMimeTypes();
    static const QMimeDatabase mimeDatabase;

    if (url.isLocalFile())
    {
        const QMimeType mimeType = mimeDatabase.mimeTypeForFile(url.fileName());
        return supportedMimeTypes.contains(mimeType.name());
    }

    QList<QMimeType> mimeTypes = mimeDatabase.mimeTypesForFileName(url.fileName());
    return mimeTypes.isEmpty() ? false : supportedMimeTypes.contains(mimeTypes.first().name());
}

bool refLoad::isSupportedClipboard()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    return mimeData ? isSupported(mimeData) : false;
}

RefImageLoader::RefImageLoader(const QUrl &url)
{
    if (url.isLocalFile())
    {
        ImageResult result = loadLocalImage(url.toLocalFile());
        if (result.isOk())
        {
            auto value = result.value();
            m_fileData = value.fileData;
            promise().addResult(value.image);
        }
        else
        {
            setError(result.error());
        }

        promise().finish();
        setFuture(promise().future());
    }
    else
    {
        m_download = std::make_unique<utils::NetworkDownload>(url);
        setFuture(m_download->future().then([this](const QByteArray &result) {
            QImage image;

            if (!m_download->errorMessage().isEmpty())
            {
                setError(m_download->errorMessage());
            }
            else if (image.loadFromData(result))
            {
                m_fileData = result;
            }
            else
            {
                const QString msg("Unable to load %1 as an image.");
                setError(msg.arg(m_download->url().toString()));
            }
            return QVariant::fromValue(image);
        }));
    }
}

RefImageLoader::RefImageLoader(const QString &filepath)
    : RefImageLoader(QUrl::fromLocalFile(filepath))
{
}

RefImageLoader::RefImageLoader(const QImage &image)
{
    setFuture(promise().future());
    if (image.isNull())
    {
        setError("Null image");
    }
    else
    {
        promise().addResult(image);
    }
    promise().finish();
}

RefImageLoader::RefImageLoader(const QPixmap &pixmap)
    : RefImageLoader(pixmap.toImage())
{}

RefImageLoader::RefImageLoader(const QByteArray &data)
{
    setFuture(promise().future());
    if (QImage image; image.loadFromData(data))
    {
        m_fileData = data;
        promise().addResult(image);
    }
    else
    {
        setError("Error loading QImage from file data");
    }
    promise().finish();
}

QImage RefImageLoader::image() const
{
    const QFuture<QVariant> thisFuture = future();
    return thisFuture.isResultReadyAt(0) ? thisFuture.result().value<QImage>() : QImage();
}
