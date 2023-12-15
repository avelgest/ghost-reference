#include "reference_loading.h"

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

} // namespace

QList<ReferenceImageSP> refLoad::fromClipboard()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData)
    {
        qCritical() << "Could get clipboard data";
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
    ReferenceImageSP refImage = getRefCollection().newReferenceImage();
    refImage->setLoader(RefImageLoader(filepath));
    refImage->setFilepath(filepath);

    return refImage;
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
        for (const auto &url : mimeData->urls())
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
    ReferenceImageSP refImage = getRefCollection().newReferenceImage();
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
        mimeType = mimeDatabase.mimeTypeForName(url.fileName());
    }
    return supportedMimeTypes.contains(mimeType.name());
}

bool refLoad::isSupportedClipboard()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    return mimeData ? isSupported(mimeData) : false;
}

QFuture<QVariant> RefLoader::future() const { return m_promise.future(); }

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
    }
    else
    {
        m_download = std::make_unique<utils::NetworkDownload>(url);
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

QFuture<QVariant> RefImageLoader::future() const
{
    if (m_download)
    {
        return m_download->future().then([](const QByteArray &result)
                                         {
            QPixmap pixmap;
            pixmap.loadFromData(result);
            return QVariant::fromValue(pixmap); });
    }
    return promise().future();
}

QPixmap RefImageLoader::pixmap() const
{
    const QFuture<QVariant> thisFuture = future();
    return thisFuture.isResultReadyAt(0) ? thisFuture.result().value<QPixmap>() : QPixmap();
}
