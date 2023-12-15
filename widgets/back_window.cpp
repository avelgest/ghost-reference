#include "back_window.h"

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

namespace
{
    const Qt::WindowFlags defaultWindowFlags = Qt::Window |
                                               Qt::FramelessWindowHint |
                                               Qt::WindowStaysOnTopHint |
                                               Qt::WindowMinimizeButtonHint | // For minimizing from the taskbar
                                               Qt::MaximizeUsingFullscreenGeometryHint;

    // Sets the WindowTransparentForInput flag without hiding the window
    void setTransparentForInput(QWidget *window, bool value)
    {
        QWindow *windowHandle = (window) ? window->windowHandle() : nullptr;

        if (windowHandle)
        {
            auto flags = windowHandle->flags();
            if (flags.testAnyFlags(Qt::WindowTransparentForInput) == value)
            {
                return;
            }
            flags.setFlag(Qt::WindowTransparentForInput, value);

            windowHandle->setFlags(flags);
            window->show();
            windowHandle->requestUpdate();
        }
    }

} // namespace

BackWindow::BackWindow(QWidget *parent)
    : QWidget(parent, defaultWindowFlags)
{
    setAttribute(Qt::WA_TranslucentBackground);
    // setWindowState(Qt::WindowMaximized);

    setGeometry(screen()->virtualGeometry());
}

void BackWindow::setWindowMode(WindowMode value)
{
    m_windowMode = value;
    setTransparentForInput(this, value == GhostMode);
}

void BackWindow::paintEvent(QPaintEvent *event)
{
// TODO Add debugging setting
#ifndef NDEBUG
    {
        QPainter painter(this);
        painter.setClipRect(event->rect());

        QPen pen(QColorConstants::Red);
        pen.setWidth(4);
        painter.setPen(pen);

        painter.drawRect(rect());
    }
#endif // NDEBUG
}

void BackWindow::showEvent([[maybe_unused]] QShowEvent *event)
{
}
