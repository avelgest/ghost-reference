#include "reference_image.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QThreadPool>

#include <QtGui/QImage>
#include <QtGui/QPainter>

#include "app.h"
#include "preferences.h"
#include "reference_collection.h"
#include "reference_loading.h"

namespace
{
    // Minimum width or height for ReferenceImage::crop (px)
    const qreal minCropAbs = 4.;
    // Minimum size of the image on screen (px)
    const qreal minDisplaySize = 96.;

    ReferenceCollection &refCollection()
    {
        return App::ghostRefInstance()->referenceItems();
    }

    QSize smallestSize(const QSize &a, const QSize &b)
    {
        return (a.width() * a.height() < b.width() * b.height()) ? a : b;
    }

    const qreal defaultEpsilon = 1e-3;

    bool nearlyEqual(qreal a, qreal b, qreal epsilon = defaultEpsilon)
    {
        return abs(a - b) <= epsilon;
    }

} // namespace

/*
Class in charge of scheduling redraws of a ReferenceImage's displayImage.
Redraws can be requested using requestRedraw and may be dropped if multiple redraws are
requested in quick succession. Redraws are performed asynchronously in a thread from the  
application thread pool.
*/
class ReferenceImageRedrawManager
{
    Q_DISABLE_COPY_MOVE(ReferenceImageRedrawManager);

public:
    class RedrawTask : public QRunnable
    {
        ReferenceImageWP m_refImage;
    public:
        explicit RedrawTask(ReferenceImageWP refImage) : m_refImage(std::move(refImage)) {}
        explicit RedrawTask(const ReferenceImageSP& refImage) : RedrawTask(refImage.toWeakRef()) {}

        void run() override
        {
            const ReferenceImageSP refImageSP = m_refImage.toStrongRef();
            refImageSP->m_redrawManager->doRedraw();
        }
    };

private:

    QPointer<ReferenceImage> m_refImage;
    std::atomic_flag m_redrawInProgress;
    std::atomic_flag m_pendingRedraw;

    // Perform the redraw. Called by RedrawTask.
    void doRedraw()
    {
        if (m_redrawInProgress.test_and_set())
        {
            return;
        }

        const ReferenceImageSP refImage = refImageSP();
        if (refImage)
        {
            refImage->redrawImage();
        }
        m_redrawInProgress.clear();

        // If another redraw was requested while this redraw was in progress
        // then start another redraw task now.
        if (m_pendingRedraw.test() && refImage)
        {
            m_pendingRedraw.clear();
            QThreadPool::globalInstance()->start(new RedrawTask(refImage));
        }
    }

public:
    explicit ReferenceImageRedrawManager(ReferenceImage* refImage)
        : m_refImage(refImage) {}
    ~ReferenceImageRedrawManager() = default;

    ReferenceImageSP refImageSP() const 
    {
        return m_refImage ? App::ghostRefInstance()->referenceItems().getReferenceImage(m_refImage->name()) : nullptr;
    }

    void requestRedraw()
    {
        if (m_refImage.isNull()) { return; }

        // If a redraw is already in progress then the redraw task should only be started
        // after that redraw has finished.
        if (m_redrawInProgress.test())
        {
            m_pendingRedraw.test_and_set();
        }
        else
        {
            QThreadPool::globalInstance()->start(new RedrawTask(refImageSP()));
        }
    }
};

ReferenceImage::ReferenceImage()
    : m_loader(new RefImageLoader()),
      m_redrawManager(new ReferenceImageRedrawManager(this)),
      m_savedAsLink(appPrefs()->getBool(Preferences::LocalFilesLink))
{
    QObject::connect(this, &ReferenceImage::settingsChanged, this, &ReferenceImage::updateDisplayImage);
}

ReferenceImage::ReferenceImage(RefImageLoaderUP &&loader)
    : ReferenceImage()
{
    setLoader(std::move(loader));
}

ReferenceImage::~ReferenceImage() = default;

void ReferenceImage::fromJson(const QJsonObject &json, RefImageLoaderUP &&loader)
{
    setName(json["name"].toString());
    setFilepath(json["filepath"].toString());
    if (loader && !loader->isError())
    {
        setLoader(std::move(loader));
    }
    else if (!m_loader && !filepath().isEmpty())
    {
        setLoader(std::make_unique<RefImageLoader>(filepath()));
    }

    setZoom(json["zoom"].toDouble(1.0));
    setSaturation(json["saturation"].toDouble(1.0));
    setSavedAsLink(json["savedAsLink"].toBool());
    setFlipHorizontal(json["flipHorizontal"].toBool(false));
    setFlipVertical(json["flipVertical"].toBool(false));
    setSmoothFiltering(json["smoothFlitering"].toBool(true));

    const QJsonArray cropArray = json["crop"].toArray();
    if (cropArray.count() == 4)
    {
        setCropF({cropArray[0].toDouble(), cropArray[1].toDouble(),
                  cropArray[2].toDouble(), cropArray[3].toDouble()});
    }

    updateDisplayImage();
}

QJsonObject ReferenceImage::toJson() const
{
    const QJsonArray cropArray({m_crop.left(), m_crop.top(), m_crop.width(), m_crop.height()});

    return {{"type", "Image"},
            {"filepath", filepath()},
            {"name", name()},
            {"crop", cropArray},
            {"zoom", zoom()},
            {"saturation", saturation()},
            {"savedAsLink", savedAsLink()},
            {"flipHorizontal", flipHorizontal()},
            {"flipVertical", flipVertical()},
            {"smoothFiltering", smoothFiltering()}};
}

const RefImageLoaderUP &ReferenceImage::loader() const
{
    return m_loader;
}

void ReferenceImage::applyRenderHints(QPainter &painter) const
{
    QPainter::RenderHints renderHints;
    const bool smooth = smoothFiltering();
    renderHints.setFlag(QPainter::SmoothPixmapTransform, smooth);

    painter.setRenderHints(renderHints);
}

void ReferenceImage::updateDisplayImage()
{
    if (!m_displayImageUpdate.test_and_set())
    {
        m_redrawManager->requestRedraw();
    }
}

qreal ReferenceImage::hoverOpacity() const
{
    return appPrefs()->getFloat(Preferences::GhostModeOpacity);
}

void ReferenceImage::setLoader(RefImageLoaderUP &&refLoader)
{
    QObject::disconnect(&m_loaderWatcher);

    m_loader = std::move(refLoader);
    if (m_loader->finished())
    {
        onLoaderFinished();
        return;
    }

    QObject::connect(&m_loaderWatcher, &LoaderWatcher::finished, this, &ReferenceImage::onLoaderFinished);
    QObject::connect(&m_loaderWatcher, &LoaderWatcher::progressValueChanged, this,
                     [this]() { emit settingsChanged(); }); // TODO Add progress signal
    // TODO Connect canceled / progressTextChanged

    m_loaderWatcher.setFuture(m_loader->future());
}

void ReferenceImage::reload()
{
    if (filepath().isEmpty())
    {
        return;
    }
    setLoader(std::make_unique<RefImageLoader>(QUrl::fromLocalFile(filepath())));
}

QSizeF ReferenceImage::minCropSize() const
{
    return {minDisplaySize / zoom(), minDisplaySize / zoom()};
}

void ReferenceImage::onLoaderFinished()
{
    const QImage image = m_loader->image();
    if (image != m_baseImage)
    {
        setBaseImage(image);
    }
    setCompressedImage(m_loader->fileData());
}

bool ReferenceImage::isValid() const
{
    return isLoaded() || (m_loader && !m_loader->isError());
}

const QString &ReferenceImage::errorMessage() const
{
    static const QString empty;
    return m_loader ? m_loader->errorMessage() : empty;
}

void ReferenceImage::setCropF(QRectF value)
{
    // Ensure there is already a valid crop (needed for clamping)
    if (!m_crop.isValid())
    {
        m_crop = m_baseImage.rect().toRectF();
    }

    // Clamp the crop to the base image
    value = value.intersected(m_baseImage.rect());
    // Clamp the crop's width/height to minimum values
    if (value.width() < minCropAbs || value.width() < minCropSize().width())
    {
        value.setCoords(cropF().left(), value.top(), cropF().right(), value.bottom());
    }
    if (value.height() < minCropAbs || value.height() < minCropSize().height())
    {
        value.setCoords(value.left(), cropF().top(), value.right(), cropF().bottom());
    }

    m_crop = value;

    emit cropChanged(crop());
}

void ReferenceImage::shiftCropF(QPointF shiftBy)
{
    const QRectF imgBounds = m_baseImage.rect().toRectF();
    QRectF newCrop = m_crop.translated(shiftBy);

    qreal adjustX = 0.;
    qreal adjustY = 0.;

    if (newCrop.left() < imgBounds.left())
    {
        adjustX = imgBounds.left() - newCrop.left();
    }
    else if (newCrop.right() > imgBounds.right())
    {
        adjustX = imgBounds.right() - newCrop.right();
    }

    if (newCrop.top() < imgBounds.top())
    {
        adjustY = imgBounds.top() - newCrop.top();
    }
    else if (newCrop.bottom() > imgBounds.bottom())
    {
        adjustY = imgBounds.bottom() - newCrop.bottom();
    }

    newCrop.translate(adjustX, adjustY);
    setCropF(newCrop);

    emit cropChanged(crop());
}

QRect ReferenceImage::displayImageCrop() const
{
    if (m_displayImage.size() == m_baseImage.size())
    {
        return crop();
    }
    const qreal dispW = static_cast<qreal>(m_displayImage.size().width());
    const qreal baseW = static_cast<qreal>(m_baseImage.size().width());
    const qreal fac = dispW / baseW;
    const QRectF srcCrop = crop().toRectF();
    return {{qRound(fac * srcCrop.left()), qRound(fac * srcCrop.top())}, displaySize()};
}

void ReferenceImage::setBaseImage(const QImage &baseImage)
{
    const QSize oldDisplaySize = m_crop.isValid() ? displaySize() : QSize();

    {
        const QMutexLocker lock(&m_baseImageMutex);
        m_baseImage = baseImage;
        setCrop(baseImage.rect());
        setDisplaySize(oldDisplaySize.isValid() ? baseImage.size().scaled(oldDisplaySize, Qt::KeepAspectRatio)
                                                : baseImage.size());
    }

    updateDisplayImage();
    emit baseImageChanged(m_baseImage);
}

const QByteArray &ReferenceImage::ensureCompressedImage()
{
    if (!m_baseImage.isNull() && m_compressedImage.isEmpty())
    {
        const int formatQuality = -1; // 0-100 (100 = least compressed, -1 = default)

        QBuffer buf(&m_compressedImage);
        buf.open(QIODevice::WriteOnly);
        m_baseImage.save(&buf, "PNG", formatQuality);
    }
    return m_compressedImage;
}

QSize ReferenceImage::displaySize() const
{
    return displaySizeF().toSize();
}

QSizeF ReferenceImage::displaySizeF() const
{
    return crop().size().toSizeF() * zoom();
}

QSize ReferenceImage::displaySizeFull() const
{
    const QSizeF dispSize = m_baseImage.size().toSizeF() * zoom();
    return {qCeil(dispSize.width()), qCeil(dispSize.height())};
}

void ReferenceImage::setDisplaySizeF(QSizeF value)
{
    value = value.expandedTo({minDisplaySize, minDisplaySize});
    const QSizeF &cropSize = cropF().size();
    const QSizeF newSize = cropSize.scaled(value, Qt::KeepAspectRatioByExpanding);
    const qreal newZoom = cropSize.isNull() ? 1.0 : newSize.width() / cropSize.width();
    setZoom(newZoom);
}

bool ReferenceImage::isLocalFile() const
{
    return !m_filepath.isEmpty();
}

void ReferenceImage::setDisplaySize(QSize value)
{
    setDisplaySizeF(value.toSizeF());
}

void ReferenceImage::setZoom(qreal value)
{
    const qreal minZoom = 0.1;
    const qreal maxZoom = 128.0;

    value = std::min(std::max(value, minZoom), maxZoom);
    m_zoom = value;
    updateDisplayImage();
    emit zoomChanged(value);
}

QTransform ReferenceImage::srcTransfrom() const
{
    QTransform transform;
    if (flipHorizontal() || flipVertical())
    {
        const QSize &destSize = m_displayImage.size();
        // Flip by scaling by -1 then translating back to the original position
        transform.scale(flipHorizontal() ? -1. : 1.,
                        flipVertical() ? -1. : 1.);
        transform.translate(flipHorizontal() ? -destSize.width() : 0.,
                            flipVertical() ? -destSize.height() : 0.);
    }
    return transform;
}

void ReferenceImage::redrawImage()
{
    m_displayImageUpdate.clear();

    // Copy the base image here in case it changes during the redraw.
    // Should be cheap due to implicit data sharing.
    QMutexLocker baseImageLock(&m_baseImageMutex);

    const QImage baseImageCopy = baseImage();
    if (baseImageCopy.isNull())
    {
        return;
    }

    const QSize dispImgSize = smallestSize(displaySizeFull(), baseImageCopy.size());

    baseImageLock.unlock();

    QImage redrawTarget;

    if (dispImgSize == baseImageCopy.size())
    {
        redrawTarget = baseImageCopy;
    }
    else
    {
        redrawTarget = baseImageCopy.scaled(dispImgSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    redrawTarget.mirror(flipHorizontal(), flipVertical());

    if (!nearlyEqual(saturation(), 1.0))
    {
        utils::reduceSaturation(redrawTarget, saturation());
    }

    const QMutexLocker displayImageLock(&m_displayImageMutex);
    m_displayImage = QPixmap::fromImage(redrawTarget);
    emit displayImageUpdated();
}

void ReferenceImage::setName(const QString &newName)
{
    App::ghostRefInstance()->referenceItems().renameReference(*this, newName);
    emit nameChanged(name());
}
