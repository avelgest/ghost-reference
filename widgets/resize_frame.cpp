#include "resize_frame.h"

#include <bit>

#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>

#include <QtCore/QMap>

#include "../app.h"
#include "back_window.h"

namespace
{
    constexpr QMargins cropMargins(const Qt::Edges edges, const QPoint diff)
    {
        QMargins margins;
        if (edges & Qt::LeftEdge)
        {
            margins.setLeft(diff.x());
        }
        else if (edges & Qt::RightEdge)
        {
            margins.setRight(-diff.x());
        }
        if (edges & Qt::TopEdge)
        {
            margins.setTop(diff.y());
        }
        else if (edges & Qt::BottomEdge)
        {
            margins.setBottom(-diff.y());
        }
        return margins;
    }

    constexpr QSize resizeSizeChange(const Qt::Edges edges, const QPoint diff) noexcept
    {
        return {(edges & Qt::LeftEdge) ? -diff.x() : diff.x(), (edges & Qt::TopEdge) ? -diff.y() : diff.y()};
    }

    constexpr Qt::Edges oppositeEdges(Qt::Edges edges) noexcept
    {
        return (edges != 0) ? ~edges : edges;
    }

    constexpr int countEdges(Qt::Edges edges) noexcept
    {
        return std::popcount(static_cast<unsigned int>(edges));
    }

    BackWindow *getBackWindow()
    {
        return App::ghostRefInstance()->backWindow();
    }

} // namespace

ResizeFrameButton::ResizeFrameButton(ResizeFrame *parent, Qt::Edges edges)
    : QAbstractButton(parent),
      m_resizeFrame(parent),
      m_edges(edges)
{
    if (parent == nullptr)
    {
        qFatal() << "parent of ResizeFrame cannot be null";
    }

    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::FocusPolicy::NoFocus);

    const int edgeCount = countEdges(edges);

    if (edgeCount == 0)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    }
    else if (edgeCount == 1)
    {
        m_isVertical = (edges == Qt::LeftEdge || edges == Qt::RightEdge);
        m_isVertical ? setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding)
                     : setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    }
    else if (edgeCount == 2)
    {
        m_isCorner = true;
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    }
    else
    {
        qCritical() << "Invalid edge for ResizeFrameButton. Value: " << m_edges.toInt();
    }

    refreshCursor();
    QObject::connect(getBackWindow(), &BackWindow::modifierKeysChanged, this, &ResizeFrameButton::onModifiersChanged);
}

ResizeFrame::TransformType ResizeFrameButton::transformType() const
{
    switch (countEdges(m_edges))
    {
    case 0:
        return m_modifiers.testFlag(Qt::ShiftModifier) ? ResizeFrame::MovingView : ResizeFrame::Moving;
    case 1:
        return ResizeFrame::Cropping;
    case 2:
        return ResizeFrame::Resizing;
    default:
        return ResizeFrame::NoTransform;
    }
}

QSize ResizeFrameButton::sizeHint() const
{
    const int boxWidth = 64;
    return {boxWidth, boxWidth};
}

void ResizeFrameButton::mousePressEvent(QMouseEvent *event)
{
    QAbstractButton::mousePressEvent(event);
    if (event->isAccepted() && isDown())
    {
        m_lastMousePos = event->globalPos();
        m_modifiers = event->modifiers();
        m_resizeFrame->setCurrentTransform(transformType());
        setMouseTracking(true);
        refreshCursor();
    }
}

void ResizeFrameButton::mouseReleaseEvent(QMouseEvent *event)
{
    QAbstractButton::mouseReleaseEvent(event);

    if (event->isAccepted() && !isDown())
    {
        m_resizeFrame->setCurrentTransform(ResizeFrame::NoTransform);
        setMouseTracking(false);
        refreshCursor();
    }
}

void ResizeFrameButton::mouseMoveEvent(QMouseEvent *event)
{
    QAbstractButton::mouseMoveEvent(event);
    const ResizeFrame::TransformType buttonTransformType = transformType();

    if (!isDown() || m_resizeFrame->currentTransform() != buttonTransformType)
    {
        event->ignore();
        return;
    }

    event->accept();

    const QPoint diff = event->globalPos() - m_lastMousePos;

    switch (buttonTransformType)
    {
    case ResizeFrame::Moving:
        emit m_resizeFrame->moved(diff);
        break;
    case ResizeFrame::Cropping:
        emit m_resizeFrame->cropped(cropMargins(m_edges, diff));
        break;
    case ResizeFrame::Resizing:
        emit m_resizeFrame->resized(oppositeEdges(m_edges), resizeSizeChange(m_edges, diff));
        break;
    case ResizeFrame::MovingView:
        emit m_resizeFrame->viewMoved(diff);
        break;
    default:
        break;
    }
    m_lastMousePos = event->globalPos();
}

void ResizeFrameButton::paintEvent([[maybe_unused]] QPaintEvent *event)
{
    // Colors for the button's border
    const QColor normalColor(Qt::white);
    const QColor pressedColor(196, 196, 0);
    const QColor hoverColor(Qt::yellow);

    if (!m_edges)
    {
        // Don't draw anything for the button in the middle of the ResizeFrame
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor borderColor(normalColor);
    if (isDown())
    {
        borderColor = pressedColor;
    }
    else if (underMouse())
    {
        borderColor = hoverColor;
    }

    QPen pen;
    pen.setColor(Qt::black);
    pen.setWidth(4);

    painter.setPen(pen);
    painter.drawRect(rect());

    pen.setColor(borderColor);
    pen.setWidth(3);

    painter.setPen(pen);
    painter.drawRect(rect());
}

void ResizeFrameButton::onModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    m_modifiers = modifiers;

    if (isVisible() && !isDown())
    {
        refreshCursor();
    }
}

void ResizeFrameButton::refreshCursor()
{
    Qt::CursorShape newCursor = Qt::ArrowCursor;
    switch (transformType())
    {
    case ResizeFrame::Moving:
        newCursor = Qt::SizeAllCursor;
        break;
    case ResizeFrame::MovingView:
        newCursor = isDown() ? Qt::ClosedHandCursor : Qt::OpenHandCursor;
        break;
    case ResizeFrame::Cropping:
        newCursor = m_isVertical ? Qt::SplitHCursor : Qt::SplitVCursor;
        break;
    case ResizeFrame::Resizing:
    {
        switch (m_edges)
        {
        case Qt::LeftEdge | Qt::TopEdge:
        case Qt::RightEdge | Qt::BottomEdge:
            newCursor = Qt::SizeFDiagCursor;
            break;
        case Qt::LeftEdge | Qt::BottomEdge:
        case Qt::RightEdge | Qt::TopEdge:
            newCursor = Qt::SizeBDiagCursor;
        default:
            break;
        }
    }
    default:
        break;
    }

    if (cursor().shape() != newCursor)
    {
        setCursor(newCursor);
    }
}

ResizeFrame::ResizeFrame(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_MouseNoMask);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setTarget(parent);

    auto *layout = new QGridLayout(this);

    layout->setSpacing(0);
    layout->setRowStretch(1, 1);
    layout->setColumnStretch(1, 1);
    layout->setContentsMargins(0, 0, 0, 0);

    // Edges
    layout->addWidget(new ResizeFrameButton(this, Qt::LeftEdge), 1, 0, Qt::AlignLeft);
    layout->addWidget(new ResizeFrameButton(this, Qt::RightEdge), 1, 2, Qt::AlignRight);
    layout->addWidget(new ResizeFrameButton(this, Qt::TopEdge), 0, 1, Qt::AlignTop);
    layout->addWidget(new ResizeFrameButton(this, Qt::BottomEdge), 2, 1, Qt::AlignBottom);

    // Corners
    layout->addWidget(new ResizeFrameButton(this, Qt::LeftEdge | Qt::TopEdge), 0, 0);
    layout->addWidget(new ResizeFrameButton(this, Qt::RightEdge | Qt::TopEdge), 0, 2);
    layout->addWidget(new ResizeFrameButton(this, Qt::LeftEdge | Qt::BottomEdge), 2, 0);
    layout->addWidget(new ResizeFrameButton(this, Qt::RightEdge | Qt::BottomEdge), 2, 2);

    // Center
    layout->addWidget(new ResizeFrameButton(this, {}), 1, 1);
}

void ResizeFrame::setCurrentTransform(TransformType transform)
{
    const TransformType oldTransform = m_currentTransform;
    m_currentTransform = transform;

    if (oldTransform != transform)
    {
        if (oldTransform != NoTransform)
        {
            emit transformFinished(oldTransform);
        }
        if (transform != NoTransform)
        {
            emit transformStarted(transform);
        }
    }

    if (transform == NoTransform)
    {
        releaseKeyboard();
    }
}

void ResizeFrame::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && currentTransform() != NoTransform)
    {
        setCurrentTransform(NoTransform);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

inline QSize ResizeFrame::sizeHint() const
{
    return (m_target) ? m_target->geometry().size() : QSize(0, 0);
}

void ResizeFrame::showOnlyMoveControl(bool value)
{
    for (int i = 0; i < layout()->count(); i++)
    {
        QLayoutItem *item = layout()->itemAt(i);
        auto *button = qobject_cast<ResizeFrameButton *>(item->widget());
        if (button && button->edges() != 0)
        {
            button->setHidden(value);
        }
    }
}
