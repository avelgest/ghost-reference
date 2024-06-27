#include "tool.h"

#include <QtCore/QTimer>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>

#include "app.h"
#include "widgets/picture_widget.h"
#include "widgets/reference_window.h"

namespace
{
    void removeEventFilterLater(QObject *obj, QObject *filterObj)
    {
        const QPointer<QObject> filterObjPtr(filterObj);

        QTimer::singleShot(0, obj, [obj, filterObjPtr]() {
            if (filterObjPtr) obj->removeEventFilter(filterObjPtr);
        });
    }
} // namespace

bool Tool::eventFilter(QObject *watched, QEvent *event)
{
    if (!watched->isWidgetType())
    {
        return false;
    }

    auto *widget = qobject_cast<QWidget *>(watched);
    Q_ASSERT(widget != nullptr);

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        mousePressEvent(widget, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::MouseMove:
        mouseMoveEvent(widget, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(widget, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::KeyPress:
        keyPressEvent(widget, static_cast<QKeyEvent *>(event));
        break;
    case QEvent::KeyRelease:
        keyReleaseEvent(widget, static_cast<QKeyEvent *>(event));
        break;
    case QEvent::ContextMenu:
        contextMenuEvent(widget, static_cast<QContextMenuEvent *>(event));
        break;
    default:
        return false;
    }

    return event->isAccepted();
}

void Tool::deactivate()
{
    if (s_activeTool == this)
    {
        onDeactivatePrivate();
        deleteLater();
        s_activeTool = nullptr;
    }
}

void Tool::setCursor(const std::optional<QCursor> &cursor)
{
    if (cursor != m_cursor)
    {
        if (isActive())
        {
            App::ghostRefInstance()->setReferenceCursor(cursor);
        }
        m_cursor = cursor;
    }
}

QWidgetList Tool::findReferenceWidgets()
{
    QWidgetList list;
    App *app = App::ghostRefInstance();
    for (const auto &refWindow : app->referenceWindows())
    {
        if (refWindow)
        {
            list.push_back(refWindow->pictureWidget());
        }
    }
    return list;
}

void Tool::contextMenuEvent([[maybe_unused]] QWidget *widget, QContextMenuEvent *event)
{
    event->accept();
}

void Tool::onActivatePrivate()
{
    App *app = App::ghostRefInstance();
    app->setGlobalMode(ToolMode);

    if (m_cursor.has_value())
    {
        app->setReferenceCursor(m_cursor);
    }

    app->backWindow()->installEventFilter(this);

    // FIXME: Need to install event filters on windows added whilst tool is active
    for (auto *widget : findReferenceWidgets())
    {
        widget->installEventFilter(this);
    }

    onActivate();
}

void Tool::onDeactivatePrivate()
{
    onDeactivate();

    App *app = App::ghostRefInstance();
    app->setGlobalMode(TransformMode);

    if (m_cursor.has_value())
    {
        app->setReferenceCursor({});
    }

    for (auto *widget : findReferenceWidgets())
    {
        removeEventFilterLater(widget, this);
    }
}