#include "extract_tool.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include "../app.h"
#include "../reference_image.h"
#include "../undo_stack.h"

#include "../widgets/picture_widget.h"
#include "../widgets/reference_window.h"

namespace
{
    const QSize minimumSelection(2, 2);
} // namespace

ExtractTool::ExtractTool()
{
    setCursor(Qt::CrossCursor);
}

void ExtractTool::drawOverlay(ReferenceWindow *refWindow, QPainter &painter)
{
    const auto *picWidget = qobject_cast<PictureWidget *>(m_target);

    if (isDragging() && picWidget && picWidget->referenceWindow() == refWindow && picWidget->image())
    {
        const QRectF dragRect(picWidget->baseImageToLocal(m_startPoint.toPointF()),
                              picWidget->baseImageToLocal(m_endPoint.toPointF()));

        QPen pen(Qt::black);
        pen.setWidth(3);

        painter.setPen(pen);
        painter.drawRect(dragRect);

        pen.setColor(Qt::white);
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawRect(dragRect);
    }
}

void ExtractTool::mouseMoveEvent(QWidget *widget, QMouseEvent *event)
{
    if (isDragging())
    {
        const auto *picWidget = qobject_cast<PictureWidget *>(widget);
        if (picWidget)
        {
            m_endPoint = picWidget->localToBaseImage(event->position()).toPoint();
            updateOverlay(widget);
            event->accept();
        }
    }
    if (!event->isAccepted())
    {
        Tool::mouseMoveEvent(widget, event);
    }
}

void ExtractTool::mousePressEvent(QWidget *widget, QMouseEvent *event)
{
    const auto *picWidget = qobject_cast<PictureWidget *>(widget);
    if (picWidget && event->button() == Qt::LeftButton)
    {
        m_startPoint = picWidget->localToBaseImage(event->position()).toPoint();
        m_endPoint = m_startPoint;
        m_target = widget;
        updateOverlay(widget);
        event->accept();
    }
}

void ExtractTool::mouseReleaseEvent(QWidget *widget, QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (extractSelection())
        {
            deactivate();
        }
        updateOverlay(widget);

        event->accept();
        m_target = nullptr;
        Q_ASSERT(!isDragging());
    }
    else
    {
        Tool::mouseReleaseEvent(widget, event);
    }
}

bool ExtractTool::isDragging() const
{
    return m_target.get() != nullptr;
}

bool ExtractTool::extractSelection() const
{
    QRect selection = QRect::span(m_startPoint, m_endPoint);
    selection.setWidth(selection.width() - 1);
    selection.setHeight(selection.height() - 1);

    const auto *picWidget = qobject_cast<PictureWidget *>(m_target);
    if (selection.isValid() && picWidget && picWidget->referenceWindow())
    {
        App *app = App::ghostRefInstance();
        app->undoStack()->pushGlobalUndo();

        const ReferenceImageSP newRefImage = picWidget->image()->duplicate(true);

        // Ensure size >= minimumSelection
        selection.setSize(selection.size().expandedTo(minimumSelection));

        newRefImage->setCrop(selection);

        ReferenceWindow *newWindow = app->newReferenceWindow();
        newWindow->addReference(newRefImage);
        newWindow->show();
        newWindow->setFocus();
        Q_ASSERT(newWindow->activeImage());

        return true;
    }

    return false;
}
