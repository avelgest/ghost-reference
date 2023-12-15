#include "picture_widget.h"

#include <QtCore/Qt>

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStackedLayout>

#include "../reference_image.h"
#include "../reference_loading.h"

#include "reference_window.h"
#include "resize_frame.h"
#include "settings_panel.h"

namespace
{
    const QSize defaultSizeHint = {256, 256};
    const qreal minOpacity = 0.1;
} // namespace

PictureWidget::PictureWidget()
    : PictureWidget(nullptr, {0})
{
}

PictureWidget::PictureWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f),
      m_resizeFrame(new ResizeFrame(this))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *layout = new QStackedLayout(this);
    layout->addWidget(m_resizeFrame);

    m_resizeFrame->setFocusProxy(this);
    m_resizeFrame->setVisible(false);

    setReferenceWindow(qobject_cast<ReferenceWindow *>(parent));
}

void PictureWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (referenceWindow())
    {
        referenceWindow()->showSettingsWindow();
    }
    else
    {
        QMenu menu;
        QAction *paste = menu.addAction("Paste", this, &PictureWidget::pasteFromClipboard);
        paste->setEnabled(refLoad::isSupportedClipboard());

        menu.exec(event->globalPos());
    }
}

void PictureWidget::enterEvent([[maybe_unused]] QEnterEvent *event)
{
    if (windowMode() == TransformMode)
    {
        m_resizeFrame->setVisible(true);
    }
}

void PictureWidget::leaveEvent([[maybe_unused]] QEvent *event)
{
    m_resizeFrame->setVisible(false);
}

void PictureWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());
    const QRectF destRect(0., 0., width(), height());

    // If there is no loaded reference image just draw a solid color
    if (m_imageSP.isNull() || !m_imageSP->isLoaded())
    {
        painter.setOpacity(std::max(minOpacity, opacityMultiplier()));
        painter.fillRect(destRect, Qt::lightGray);
        return;
    }

    ReferenceImage &refImage = *m_imageSP;
    const QImage &dispImage = refImage.displayImage();

    // dispImage should be the same size as this->rect()
    if (destRect.size() != dispImage.size())
    {
        qDebug() << "Size mismatch when drawing image" << refImage.name();
    }

    painter.setOpacity(std::max(minOpacity, refImage.opacity() * opacityMultiplier()));
    painter.drawImage(destRect, dispImage);
}

void PictureWidget::pasteFromClipboard() const
{
    // TODO Support pasting multiple
    const QList<ReferenceImageSP> newRefImages = refLoad::fromClipboard();

    if (newRefImages.isEmpty())
    {
        qWarning() << "Unable to load any reference images from the clipboard";
        return;
    }

    for (const auto &refImage : newRefImages)
    {
        referenceWindow()->addReference(refImage);
    }
}

QSize PictureWidget::sizeHint() const
{
    return (m_imageSP && m_imageSP->isLoaded()) ? m_imageSP->displaySize() : defaultSizeHint;
}

void PictureWidget::setImage(const ReferenceImageSP &image)
{
    if (m_imageSP)
    {
        m_imageSP->disconnect(this);
    }
    m_imageSP = image;

    if (image)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        QObject::connect(image.get(), &ReferenceImage::cropChanged,
                         this, &PictureWidget::updateGeometry);
        QObject::connect(image.get(), &ReferenceImage::zoomChanged,
                         this, &PictureWidget::updateGeometry);

        QObject::connect(image.get(), &ReferenceImage::displayImageUpdate, [this]()
                         { update(); });
        QObject::connect(image.get(), &ReferenceImage::settingsChanged, [this]()
                         { update(); });
    }

    updateGeometry();
    update();
}

void PictureWidget::setReferenceWindow(ReferenceWindow *refWindow)
{
    if (m_referenceWindow)
    {
        m_referenceWindow->disconnect(this);
    }
    m_referenceWindow = refWindow;
    QObject::connect(m_referenceWindow, &ReferenceWindow::activeImageChanged, this, &PictureWidget::setImage);
}

WindowMode PictureWidget::windowMode() const
{
    if (m_referenceWindow)
    {
        return m_referenceWindow->windowMode();
    }
    return TransformMode;
}
