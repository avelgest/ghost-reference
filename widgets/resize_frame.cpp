#include "resize_frame.h"

#include <bit>

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

#include <QtCore/QMap>

namespace
{
    Qt::CursorShape cursorForEdge(Qt::Edges edge, Qt::KeyboardModifiers modifiers)
    {
        static const QMap<Qt::Edges, Qt::CursorShape> s_edgeMap{
            {Qt::TopEdge, Qt::SplitVCursor},
            {Qt::BottomEdge, Qt::SplitVCursor},
            {Qt::LeftEdge, Qt::SplitHCursor},
            {Qt::RightEdge, Qt::SplitHCursor},
            {Qt::TopEdge | Qt::LeftEdge, Qt::SizeFDiagCursor},
            {Qt::BottomEdge | Qt::RightEdge, Qt::SizeFDiagCursor},
            {Qt::TopEdge | Qt::RightEdge, Qt::SizeBDiagCursor},
            {Qt::BottomEdge | Qt::LeftEdge, Qt::SizeBDiagCursor},
        };

        if (auto found = s_edgeMap.constFind(edge); found != s_edgeMap.constEnd())
        {
            return found.value();
        }
        return (modifiers & Qt::ShiftModifier) ? Qt::OpenHandCursor : Qt::SizeAllCursor;
    }

    Qt::Edges oppositeEdges(Qt::Edges edges)
    {
        return (edges != 0) ? ~edges : edges;
    }
} // namespace

ResizeFrame::ResizeFrame(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    // setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setTarget(parent);

    setCursor(cursorForEdge({}, {}));
}

void ResizeFrame::setCurrentTransform(TransformType transform)
{
    m_currentTransform = transform;
    if (transform == NoTransform)
    {
        releaseKeyboard();
    }
}

Qt::Edges ResizeFrame::hotspotAtPoint(const QPointF pos) const
{
    const qreal sideSize = 0.1;

    const QPointF normPos(pos.x() / size().width(),
                          pos.y() / size().height());

    Qt::Edges edges;
    if (normPos.x() < sideSize)
    {
        edges |= Qt::LeftEdge;
    }
    else if (normPos.x() > 1.0 - sideSize)
    {
        edges |= Qt::RightEdge;
    }

    if (normPos.y() < sideSize)
    {
        edges |= Qt::TopEdge;
    }
    else if (normPos.y() > 1.0 - sideSize)
    {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

QMarginsF ResizeFrame::relativeMargins(const Qt::Edges edges, const QPoint diff) const
{
    const QPointF floatDiff = diff.toPointF();
    QMarginsF margins(1.0, 1.0, 1.0, 1.0);

    if (edges & Qt::LeftEdge)
    {
        margins.setLeft(1.0 + floatDiff.x() / width());
    }
    else if (edges & Qt::RightEdge)
    {
        margins.setRight(1.0 - floatDiff.x() / width());
    }
    if (edges & Qt::TopEdge)
    {
        margins.setTop(1.0);
    }
    else if (edges & Qt::BottomEdge)
    {
        margins.setBottom(1.0);
    }
    return margins;
}

QMargins cropMargins(const Qt::Edges edges, const QPoint diff)
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

void ResizeFrame::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const int numEdges = std::popcount(static_cast<unsigned>(m_currentEdges));

        event->accept();
        grabKeyboard();

        TransformType transformType = NoTransform;

        switch (numEdges)
        {
        case 0:
            transformType = (event->modifiers() & Qt::ShiftModifier) ? MovingView : Moving;
            break;
        case 1:
            transformType = Cropping;
            break;
        case 2:
            transformType = Resizing;
            break;
        default:
            transformType = NoTransform;
        }
        setCurrentTransform(transformType);
    }
    else
    {
        event->ignore();
    }
}

void ResizeFrame::mouseMoveEvent(QMouseEvent *event)
{
    static Qt::KeyboardModifiers lastModifiers;

    if (!event->buttons().testFlag(Qt::LeftButton))
    {
        setCurrentTransform(NoTransform);
    }

    if (currentTransform() == NoTransform)
    {
        const Qt::Edges edges = hotspotAtPoint(event->position());
        if (edges != m_currentEdges || lastModifiers != event->modifiers())
        {
            setCursor(cursorForEdge(edges, event->modifiers()));
            m_currentEdges = edges;
            lastModifiers = event->modifiers();
        }
    }
    else
    {
        event->accept();
        const QPoint diff = event->globalPos() - m_lastMousePos;

        switch (currentTransform())
        {
        case Moving:
            emit moved(diff);
            break;
        case Cropping:
            emit cropped(cropMargins(m_currentEdges, diff));
            break;
        case Resizing:
        {
            const QSize sizeChange((m_currentEdges & Qt::LeftEdge) ? -diff.x() : diff.x(),
                                   (m_currentEdges & Qt::TopEdge) ? -diff.y() : diff.y());
            emit resized(oppositeEdges(m_currentEdges),
                         targetSize() + sizeChange);
            break;
        }
        case MovingView:
            emit viewMoved(diff);
            break;
        default:
            break;
        }
    }
    m_lastMousePos = event->globalPos();
}

void ResizeFrame::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        setCurrentTransform(NoTransform);
        event->accept();
    }
    else
    {
        event->ignore();
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

void ResizeFrame::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(event->rect());

    const QColor frameColor(0, 0, 255);
    QPen framePen(frameColor);
    framePen.setWidth(3);

    const int width = size().width();
    const int height = size().height();

    // Draw the frame
    painter.setPen(framePen);
    painter.drawRect(0, 0, width - 1, height - 1);

    // Draw boxes in the corners and on the edges
    const QSize boxSize(14, 14);
    QRect box({0, 0}, boxSize);

    // Draw boxes by moving box across the widget
    for (int i = 0; i < (3 * 3); i++)
    {
        if (i != 4) // Don't draw a box in the middle
        {
            painter.fillRect(box, frameColor);
        }

        if ((i + 1) % 3 == 0) // i == 2 and i == 5
        {
            // Move box down to the start of the next row
            box.moveTo(0, box.top() + (height - boxSize.height()) / 2);
        }
        else
        {
            // Move box to the middle, then to the end of the row
            box.moveLeft(box.left() + (width - boxSize.width()) / 2);
        }
    }
}

inline QSize ResizeFrame::sizeHint() const
{
    return (m_target) ? m_target->geometry().size() : QSize(0, 0);
}
