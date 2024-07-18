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

namespace
{
    class LoadException : QException
    {
        void raise() const override { throw *this; }
        LoadException *clone() const override { return new LoadException(*this); }
    };

    using PixmapResult = utils::result<QPixmap, QString>;

    PixmapResult loadLocalPixmap(const QString &filepath)
    {
        QPixmap pixmap;
        if (!pixmap.load(filepath))
        {
            const QString msg("Unable to load file %1");
            return PixmapResult::Err(msg.arg(filepath));
        }
        return pixmap;
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

ReferenceImageSP refLoad::fromPixmap(const QPixmap &pixmap)
{
    ReferenceImageSP refImage = getRefCollection().newReferenceImage();
    refImage->setLoader(RefImageLoader(pixmap));

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
        return {refLoad::fromPixmap(QPixmap::fromImage(qImage))};
    }

    return {};
}

ReferenceImageSP refLoad::fromUrl(const QUrl &url)
{
    const QString name = stripExt(url.fileName());
    ReferenceImageSP refImage = getRefCollection().newReferenceImage(name);
    refImage->setLoader(RefImageLoader(url));

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

bool refLoad::isSupported(const QDragEnterEvent *event)
{
    return isSupported(event->mimeData());
}

bool refLoad::isSupported(const QUrl &url)
{
    static const QList<QByteArray> supportedMimeTypes = QImageReader::supportedMimeTypes();
    static const QMimeDatabase mimeDatabase;

    QMimeType mimeType;
    if (url.isLocalFile())
    {
        mimeType = mimeDatabase.mimeTypeForFile(url.fileName());
    }
    else
    {
        mimeType = mimeDatabase.mimeTypesForFileName(url.fileName()).first();
    }
    return supportedMimeTypes.contains(mimeType.name());
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
        PixmapResult result = loadLocalPixmap(url.toLocalFile());
        if (result.isOk())
        {
            promise().addResult(std::move(result.value()));
        }

        promise().finish();
        setFuture(promise().future());
    }
    else
    {
        m_download = std::make_unique<utils::NetworkDownload>(url);
        setFuture(m_download->future().then([](const QByteArray &result) {
            QPixmap pixmap;
            pixmap.loadFromData(result);
            return QVariant::fromValue(pixmap);
        }));
    }
}

RefImageLoader::RefImageLoader(const QString &url)
    : RefImageLoader(QUrl(url))
{
}

RefImageLoader::RefImageLoader(const QImage &image)
    : RefImageLoader(QPixmap::fromImage(image))
{
}

RefImageLoader::RefImageLoader(const QPixmap &pixmap)
{
    setFuture(promise().future());
    if (pixmap.isNull())
    {
        promise().finish();
    }
    else
    {
        promise().addResult(pixmap);
        promise().finish();
    }
}

QPixmap RefImageLoader::pixmap() const
{
    const QFuture<QVariant> thisFuture = future();
    return thisFuture.isResultReadyAt(0) ? thisFuture.result().value<QPixmap>() : QPixmap();
}
