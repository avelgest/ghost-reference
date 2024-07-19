#include "picture_widget.h"

#include <array>

#include <QtCore/Qt>

#include <QtGui/QBitmap>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStackedLayout>

#include "../app.h"
#include "../reference_image.h"
#include "../reference_loading.h"

#include "reference_window.h"
#include "resize_frame.h"
#include "settings_panel.h"

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

    // Called when the picture widget has no reference image to display
    void drawNoImageMessage(QPainter &painter, const QRect &rect)
    {
        const int fontSize = 24;
        const QString message = "Drag and drop an image here";
        const int textMargin = 8;
        const QMargins margins(textMargin, textMargin, textMargin, textMargin);

        QFont font = painter.font();
        font.setPointSize(fontSize);
        painter.setFont(font);

        painter.drawText(rect.marginsRemoved(margins), Qt::AlignCenter | Qt::TextWordWrap, message);
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

    // If there is no loaded reference image just draw a message on a solid color
    if (m_imageSP.isNull() || !m_imageSP->isLoaded())
    {
        painter.setOpacity(std::max(minOpacity, opacityMultiplier()));
        painter.fillRect(destRect, Qt::lightGray);
        drawNoImageMessage(painter, rect());
        return;
    }

    ReferenceImage &refImage = *m_imageSP;
    const QImage &dispImage = refImage.displayImage();

    // Draw a checkered background for images with alpha
    if (refImage.baseImage().hasAlphaChannel() && referenceWindow()->windowMode() != GhostMode)
    {
        if (referenceWindow())
        {
            // Draw relative to the reference window's parent (looks better when cropping)
            painter.setBrushOrigin(-referenceWindow()->pos());
        }
        drawCheckerBoard(painter, event->rect());
    }

    // dispImage should be the same size as this->rect()
    if (destRect.size() != dispImage.size())
    {
        qWarning() << "Size mismatch when drawing image" << refImage.name();
    }

    painter.setOpacity(std::max(minOpacity, refImage.opacity() * opacityMultiplier()));
    painter.drawImage(destRect, dispImage);
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
        m_resizeFrame->setVisible(underMouse());
    }
    else
    {
        m_resizeFrame->hide();
    }
}

QPointF PictureWidget::localToImage(const QPointF &localPos) const
{
    return localPos;
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

        QObject::connect(image.get(), &ReferenceImage::displayImageUpdate, this, [this]() { update(); });
        QObject::connect(image.get(), &ReferenceImage::settingsChanged, this, [this]() { update(); });
    }
    m_resizeFrame->showOnlyMoveControl(m_imageSP.isNull());

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
    QObject::connect(m_referenceWindow, &ReferenceWindow::windowModeChanged, this, [this]() { update(); });
}

WindowMode PictureWidget::windowMode() const
{
    if (m_referenceWindow)
    {
        return m_referenceWindow->windowMode();
    }
    return TransformMode;
}
