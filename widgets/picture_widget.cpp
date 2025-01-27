#include "picture_widget.h"

#include <array>

#include <QtCore/Qt>

#include <QtGui/QBitmap>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStackedLayout>

#include "../app.h"
#include "../reference_image.h"
#include "../reference_loading.h"

#include "reference_window.h"
#include "resize_frame.h"

namespace
{
    const QSize defaultSizeHint = {256, 256};
    const qreal minOpacity = 0.1;

    // Draw a checker board pattern (for when drawing images with an alpha channel)
    void drawCheckerBoard(QPainter &painter, const QRect &rect)
    {
        const int squareSize = 24;
        const QColor color1("gray");
        const QColor color2("darkgray");

        // Create a 2*2 QBitmap texture with alternating values then scale it to squareSize
        constexpr std::array<const uchar, 3> texBits = {1, 2, 0}; // bitmap rows (in binary): 01 10
        static const QTransform texTransform = QTransform::fromScale(squareSize / 2., squareSize / 2.);
        static const QBitmap tex = QBitmap::fromData({2, 2}, texBits.data()).transformed(texTransform);

        QBrush brush;
        brush.setColor(color2);
        brush.setTexture(tex);

        QPen pen;
        pen.setWidth(0);

        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing, false);

        painter.fillRect(rect, color1);
        painter.fillRect(rect, brush);
    }

    void drawMessage(QPainter &painter, const QRect &rect, const QString &msg)
    {
        const int fontSize = 24;
        const int textMargin = 8;
        const QMargins margins(textMargin, textMargin, textMargin, textMargin);

        QFont font = painter.font();
        font.setPointSize(fontSize);
        painter.setFont(font);

        painter.drawText(rect.marginsRemoved(margins), Qt::AlignCenter | Qt::TextWordWrap, msg);
    }

    void toggleGlobalGhostMode()
    {
        App *app = App::ghostRefInstance();
        switch (app->globalMode())
        {
        case WindowMode::GhostMode:
            app->setGlobalMode(WindowMode::TransformMode);
            break;
        case WindowMode::TransformMode:
            app->setGlobalMode(WindowMode::GhostMode);
            break;
        default:
            break;
        }
    }

    // Check if a widget is under the mouse. Like QWidget::underMouse but works in more situations.
    bool isUnderMouse(const QWidget *widget)
    {
        return widget->rect().contains(widget->mapFromGlobal(QCursor::pos()));
    }

} // namespace

PictureWidget::PictureWidget(QWidget *parent)
    : QWidget(parent),
      m_resizeFrame(new ResizeFrame(this))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *layout = new QStackedLayout(this);
    layout->addWidget(m_resizeFrame);

    m_resizeFrame->setFocusProxy(this);
    m_resizeFrame->setVisible(false);

    if (auto *refWindow = qobject_cast<ReferenceWindow *>(parent); refWindow)
    {
        setReferenceWindow(refWindow);
        QObject::connect(refWindow, &ReferenceWindow::windowModeChanged, this, &PictureWidget::onWindowModeChanged);
    }

    App *app = App::ghostRefInstance();
    QObject::connect(app, &App::referenceCursorChanged, this, &PictureWidget::onReferenceCursorChanged);
}

bool PictureWidget::isCacheInvalidated() const
{
    return m_cacheInvalidated || m_cachedImage.isNull() || m_cachedImage.size() != size();
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

void PictureWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        toggleGlobalGhostMode();
    }
    else
    {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void PictureWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());
    const QRectF destRect(0., 0., width(), height());

    const qreal opacity = m_referenceWindow ? m_referenceWindow->opacity() : 1.0;
    painter.setOpacity(std::max(minOpacity, opacity) * opacityMultiplier());

    // If there is no vaild reference image loaded just draw a message on a solid color
    if (m_imageSP.isNull() || !m_imageSP->isLoaded())
    {
        // If an error occurred during loading then draw the error message
        const QString msg = (m_imageSP && !m_imageSP->errorMessage().isEmpty()) ? m_imageSP->errorMessage()
                                                                                : "Drag and drop an image here";
        painter.setOpacity(std::max(minOpacity, opacityMultiplier()));
        painter.fillRect(destRect, Qt::lightGray);
        drawMessage(painter, rect(), msg);
        return;
    }

    // Draw a checkered background for images with alpha
    if (m_imageSP->hasAlpha() && referenceWindow()->windowMode() != GhostMode)
    {
        if (referenceWindow())
        {
            // Draw relative to the reference window's parent (looks better when cropping)
            painter.setBrushOrigin(-referenceWindow()->pos());
        }
        drawCheckerBoard(painter, event->rect());
    }

    // Redraw m_cachedImage if necessary
    if (isCacheInvalidated())
    {
        ReferenceImage &refImage = *m_imageSP;

        if (m_cachedImage.size() != size()) m_cachedImage = QPixmap(size());

        if (refImage.hasAlpha()) m_cachedImage.fill(Qt::transparent);

        QPainter cachePainter(&m_cachedImage);
        cachePainter.setCompositionMode(refImage.hasAlpha() ? QPainter::CompositionMode_SourceOver
                                                            : QPainter::CompositionMode_Source);

        const auto lock = refImage.lockDisplayImage();
        const QPixmap &dispImage = refImage.displayImage();

        cachePainter.setRenderHint(QPainter::SmoothPixmapTransform, refImage.smoothFiltering());

        cachePainter.drawPixmap(destRect, dispImage, refImage.displayImageCrop());
        m_cacheInvalidated = false;
    }

    // Draw m_cachedImage to the screen
    painter.drawPixmap(0, 0, m_cachedImage);
}

void PictureWidget::onReferenceCursorChanged(const std::optional<QCursor> &cursor, std::optional<RefType> refType)
{
    if (!refType.has_value() || refType == RefType::Image)
    {
        setCursor(cursor.has_value() ? *cursor : QCursor());
    }
}

void PictureWidget::onWindowModeChanged(WindowMode newMode)
{
    if (newMode == TransformMode)
    {
        m_resizeFrame->setVisible(isUnderMouse(this));
    }
    else
    {
        m_resizeFrame->hide();
    }
}

QPointF PictureWidget::localToBaseImage(const QPointF &localPos) const
{
    if (!m_imageSP) return localPos;
    const QRect crop = m_imageSP->crop();
    const qreal sizeRatio = crop.width() / static_cast<qreal>(width());

    QPointF pos = localPos;
    if (m_imageSP->flipHorizontal()) pos.setX(width() - localPos.x());
    if (m_imageSP->flipVertical()) pos.setY(height() - localPos.y());

    return pos * sizeRatio + crop.topLeft().toPointF();
}

QPointF PictureWidget::localToDisplayImage(const QPointF &localPos) const
{
    if (!m_imageSP) return localPos;
    const QRect dispCrop = m_imageSP->displayImageCrop();
    const qreal sizeRatio = dispCrop.width() / static_cast<qreal>(width());

    return localPos * sizeRatio + dispCrop.topLeft().toPointF();
}

QPointF PictureWidget::baseImageToLocal(const QPointF &basePos) const
{
    if (!m_imageSP) return basePos;
    const QRect crop = m_imageSP->crop();
    const qreal sizeRatio = width() / static_cast<qreal>(crop.width());

    QPointF pos = basePos;
    if (m_imageSP->flipHorizontal()) pos.setX(width() - basePos.x());
    if (m_imageSP->flipVertical()) pos.setY(height() - basePos.y());

    return pos * sizeRatio - crop.topLeft().toPointF();
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

        QObject::connect(image.get(), &ReferenceImage::cropChanged, this, [this]() {
            updateGeometry();
            invalidateCache();
        });
        QObject::connect(image.get(), &ReferenceImage::zoomChanged,
                         this, &PictureWidget::updateGeometry);

        QObject::connect(image.get(), &ReferenceImage::displayImageUpdated, this, [this]() { invalidateCache(); });
        QObject::connect(image.get(), &ReferenceImage::settingsChanged, this, [this]() { invalidateCache(); });

        QObject::connect(image.get(), &ReferenceImage::baseImageChanged, this,
                         [this]() { m_resizeFrame->showOnlyMoveControl(!m_imageSP || !m_imageSP->isLoaded()); });
    }
    m_resizeFrame->showOnlyMoveControl(!m_imageSP || !m_imageSP->isLoaded());

    invalidateCache();
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
    setImage(refWindow->activeImage());
    QObject::connect(m_referenceWindow, &ReferenceWindow::activeImageChanged, this, &PictureWidget::setImage);
    QObject::connect(m_referenceWindow, &ReferenceWindow::windowModeChanged, this, [this]() { invalidateCache(); });
}

WindowMode PictureWidget::windowMode() const
{
    if (m_referenceWindow)
    {
        return m_referenceWindow->windowMode();
    }
    return TransformMode;
}

void PictureWidget::invalidateCache()
{
    m_cacheInvalidated = true;
    update();
}
