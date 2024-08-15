#pragma once

#include <algorithm>
#include <memory>

#include <QtCore/QFutureWatcher>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <QtGui/QImage>
#include <QtGui/QPixmap>

#include "reference_loading.h"
#include "types.h"

class ReferenceImage : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReferenceImage)

    friend class ReferenceCollection;

    using LoaderWatcher = QFutureWatcher<QVariant>;

    std::unique_ptr<RefImageLoader> m_loader;
    LoaderWatcher m_loaderWatcher;

    QByteArray m_compressedImage;
    QPixmap m_baseImage;
    QImage m_displayImage;
    bool m_displayImageUpdate = true;

    QString m_filepath;
    QString m_name;
    QRectF m_crop;
    qreal m_zoom = 1.0;
    qreal m_opacity = 1.0;
    qreal m_saturation = 1.0;
    bool m_savedAsLink = false;
    bool m_filpHorizontal = false;
    bool m_flipVertical = false;
    bool m_smoothFiltering = true;

public:
    ~ReferenceImage() override = default;

    void setLoader(RefImageLoaderUP &&RefLoader);

    void reload();

    void fromJson(const QJsonObject &json, RefImageLoaderUP &&loader);
    QJsonObject toJson() const;

    void applyRenderHints(QPainter &painter) const;
    void updateDisplayImage();

    const QString &filepath() const;
    void setFilepath(const QString &filepath);

    const QPixmap &baseImage() const;
    void setBaseImage(const QPixmap &baseImage);

    const QByteArray &compressedImage() const;
    const QByteArray &ensureCompressedImage();
    void setCompressedImage(const QByteArray &value);
    void setCompressedImage(QByteArray &&value);

    const QImage &displayImage();

    const QString &name() const;
    void setName(const QString &newName);

    QRect crop() const;
    void setCrop(QRect value);

    QRectF cropF() const;
    void setCropF(QRectF value);
    void shiftCropF(QPointF shiftBy);

    bool isLoaded() const;
    const QString &errorMessage() const;

    /*The size (in px) this image should be displayed at.*/
    QSize displaySize() const;
    /*Sets the zoom to give the specified display size. If value has a different aspect ratio from
    this image then it will be expanded to the same aspect ratio.*/
    void setDisplaySize(QSize value);
    // QSizeF displaySizeF() const;
    void setDisplaySizeF(QSizeF value);

    bool isLocalFile() const;
    // Should this image should be stored only as a link to a local file when saving the session.
    bool savedAsLink() const;
    void setSavedAsLink(bool value);

    bool flipHorizontal() const;
    void setFlipHorizontal(bool value);

    bool flipVertical() const;
    void setFlipVertical(bool value);

    qreal hoverOpacity() const;

    qreal opacity() const;
    void setOpacity(qreal value);

    qreal saturation() const;
    void setSaturation(qreal value);

    bool smoothFiltering() const;
    void setSmoothFiltering(bool value);

    qreal zoom() const;
    void setZoom(qreal value);

signals:
    void baseImageChanged(QPixmap &baseImage);
    void cropChanged(QRect newCrop);
    void displayImageUpdate();
    void filepathChanged(const QString &newValue);
    void nameChanged(const QString &newValue);
    void settingsChanged();
    void zoomChanged(qreal newValue);

private:
    // Constructors should only be called from a ReferenceCollection
    ReferenceImage();
    explicit ReferenceImage(RefImageLoaderUP &&loader);

    QSizeF minCropSize() const;
    void onLoaderFinished();
    QTransform srcTransfrom() const;
    void redrawImage();
};

// inline definitions
inline void ReferenceImage::updateDisplayImage()
{
    if (!m_displayImageUpdate)
    {
        m_displayImageUpdate = true;
        emit displayImageUpdate();
    }
}

inline const QString &ReferenceImage::filepath() const { return m_filepath; }

inline void ReferenceImage::setFilepath(const QString &filepath)
{
    m_filepath = filepath;
    emit filepathChanged(m_filepath);
}

inline const QImage &ReferenceImage::displayImage()
{
    if (m_displayImageUpdate)
    {
        redrawImage();
    }
    return m_displayImage;
}

inline const QPixmap &ReferenceImage::baseImage() const { return m_baseImage; }

inline const QByteArray &ReferenceImage::compressedImage() const { return m_compressedImage; }

inline void ReferenceImage::setCompressedImage(const QByteArray &value) { m_compressedImage = value; }

inline void ReferenceImage::setCompressedImage(QByteArray &&value) { m_compressedImage = std::move(value); }

inline const QString &ReferenceImage::name() const
{
    return m_name;
}

inline QRect ReferenceImage::crop() const
{
    const qreal epsilon = 1e-6;
    // return m_crop.toRect();
    return {qFloor(m_crop.left() + epsilon),
            qFloor(m_crop.top() + epsilon),
            qCeil(m_crop.width() - epsilon),
            qCeil(m_crop.height() - epsilon)};
}

inline bool ReferenceImage::isLoaded() const { return !m_baseImage.isNull(); }

inline void ReferenceImage::setCrop(QRect value)
{
    setCropF(value.toRectF());
}

inline bool ReferenceImage::savedAsLink() const
{
    return m_savedAsLink && !m_filepath.isEmpty();
}

inline void ReferenceImage::setSavedAsLink(bool value)
{
    m_savedAsLink = value;
}

inline QRectF ReferenceImage::cropF() const { return m_crop; }

inline bool ReferenceImage::flipHorizontal() const { return m_filpHorizontal; }

inline void ReferenceImage::setFlipHorizontal(bool value)
{
    m_filpHorizontal = value;
    updateDisplayImage();
}

inline bool ReferenceImage::flipVertical() const { return m_flipVertical; }

inline void ReferenceImage::setFlipVertical(bool value)
{
    m_flipVertical = value;
    updateDisplayImage();
}

inline qreal ReferenceImage::opacity() const { return m_opacity; }

inline void ReferenceImage::setOpacity(qreal value)
{
    m_opacity = std::clamp(value, 0., 1.);
    emit settingsChanged();
}

inline qreal ReferenceImage::saturation() const { return m_saturation; }

inline void ReferenceImage::setSaturation(qreal value)
{
    m_saturation = std::clamp(value, 0., 1.);
    updateDisplayImage();
    emit settingsChanged();
}

inline bool ReferenceImage::smoothFiltering() const { return m_smoothFiltering; }
inline void ReferenceImage::setSmoothFiltering(bool value)
{
    m_smoothFiltering = value;
    emit settingsChanged();
}

inline qreal ReferenceImage::zoom() const { return m_zoom; }
