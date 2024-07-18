#include "reference_image.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

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

    void adjustSaturation(QImage &image, qreal saturation)
    {
        const auto saturationF = static_cast<float>(saturation);
        for (int y = 0; y < image.height(); y++)
        {
            QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); x++)
            {
                const QColor color(line[x]);
                QColor hsv = color.toHsv();
                hsv.setHsvF(hsv.hueF(), saturationF * hsv.saturationF(), hsv.valueF(), hsv.alphaF());
                line[x] = hsv.rgba();
            }
        }
    }

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

ReferenceImage::ReferenceImage()
    : m_loader(new RefImageLoader())
{
    QObject::connect(this, &ReferenceImage::settingsChanged, this, &ReferenceImage::updateDisplayImage);
}

ReferenceImage::ReferenceImage(RefImageLoader &&loader)
    : ReferenceImage()
{
    setLoader(std::move(loader));
}

void ReferenceImage::fromJson(const QJsonObject &json, RefImageLoader &&loader)
{
    setName(json["name"].toString());
    setFilepath(json["filepath"].toString());
    if (loader.isValid())
    {
        setLoader(std::move(loader));
    }
    else if (!filepath().isEmpty())
    {
        setLoader(RefImageLoader(filepath()));
    }

    setZoom(json["zoom"].toDouble(1.0));
    setOpacity(json["opacity"].toDouble(1.0));
    setSaturation(json["saturation"].toDouble(1.0));
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
            {"opacity", opacity()},
            {"saturation", saturation()},
            {"flipHorizontal", flipHorizontal()},
            {"flipVertical", flipVertical()},
            {"smoothFiltering", smoothFiltering()}};
}

void ReferenceImage::applyRenderHints(QPainter &painter) const
{
    QPainter::RenderHints renderHints;
    const bool smooth = smoothFiltering();
    renderHints.setFlag(QPainter::SmoothPixmapTransform, smooth);

    painter.setRenderHints(renderHints);
}

qreal ReferenceImage::hoverOpacity() const
{
    return appPrefs()->getFloat(Preferences::GhostModeOpacity);
}

void ReferenceImage::setLoader(RefImageLoader &&refLoader)
{
    QObject::disconnect(&m_loaderWatcher);

    *m_loader = std::move(refLoader);
    if (m_loader->finished())
    {
        setBaseImage(m_loader->pixmap());
        return;
    }

    QObject::connect(&m_loaderWatcher, &LoaderWatcher::finished, [this]()
                     { setBaseImage(m_loaderWatcher.result().value<QPixmap>()); });
    QObject::connect(&m_loaderWatcher, &LoaderWatcher::progressValueChanged, [this]()
                     { emit settingsChanged(); }); // TODO Add progress signal
    // TODO Connect canceled / progressTextChanged

    m_loaderWatcher.setFuture(m_loader->future());
}

QSizeF ReferenceImage::minCropSize() const
{
    return {minDisplaySize / zoom(), minDisplaySize / zoom()};
}

void ReferenceImage::setCropF(QRectF value)
{
    // Ensure there is already a valid crop (needed for clamping)
    if (cropF().isNull())
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

    updateDisplayImage();
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

    updateDisplayImage();
    emit cropChanged(crop());
}

void ReferenceImage::setBaseImage(const QPixmap &baseImage)
{
    const QSize oldDisplaySize = displaySize();

    m_baseImage = baseImage;

    setCrop(baseImage.rect());
    setDisplaySize(oldDisplaySize.isNull() ? baseImage.size() : oldDisplaySize);
    updateDisplayImage();
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
    return (crop().size().toSizeF() * zoom()).toSize();
}

void ReferenceImage::setDisplaySizeF(QSizeF value)
{
    value = value.expandedTo({minDisplaySize, minDisplaySize});
    const QSizeF &cropSize = cropF().size();
    const QSizeF newSize = cropSize.scaled(value, Qt::KeepAspectRatioByExpanding);
    setZoom(newSize.width() / cropSize.width());
}

void ReferenceImage::setDisplaySize(QSize value)
{
    setDisplaySizeF(value.toSizeF());
}

void ReferenceImage::setZoom(qreal value)
{
    const qreal minZoom = 0.1;
    // const qreal maxZoom = 16.0;

    value = std::max(value, minZoom);
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
    m_displayImageUpdate = false;
    if (m_baseImage.isNull())
    {
        if (!m_displayImage.isNull())
        {
            m_displayImage = QImage();
        }
        return;
    }

    const QSize dispImgSize = smallestSize(displaySize(), crop().size());

    if (m_displayImage.isNull() || m_displayImage.size() != dispImgSize)
    {
        m_displayImage = QImage(dispImgSize, QImage::Format_ARGB32_Premultiplied);
        // m_displayImage.fill(0U);
    }

    {
        QPainter painter(&m_displayImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setTransform(srcTransfrom());
        painter.drawPixmap(m_displayImage.rect(), m_baseImage, crop());
    }

    if (!nearlyEqual(saturation(), 1.0))
    {
        adjustSaturation(m_displayImage, saturation());
    }

    // Scale the image to displaySize
    if (dispImgSize != displaySize())
    {
        const Qt::TransformationMode filtering = smoothFiltering() ? Qt::SmoothTransformation
                                                                   : Qt::FastTransformation;
        m_displayImage = m_displayImage.scaled(displaySize(), Qt::IgnoreAspectRatio, filtering);
    }
}

void ReferenceImage::setName(const QString &newName)
{
    App::ghostRefInstance()->referenceItems().renameReference(*this, newName);
    emit nameChanged(name());
}
