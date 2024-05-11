#include "back_window.h"

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

#include "../utils/window_utils.h"

namespace
{
    const Qt::WindowFlags defaultWindowFlags = Qt::Window |
                                               Qt::FramelessWindowHint |
                                               Qt::WindowStaysOnTopHint |
                                               Qt::WindowMinimizeButtonHint | // For minimizing from the taskbar
                                               Qt::MaximizeUsingFullscreenGeometryHint;

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
    utils::setTransparentForInput(this, value == GhostMode);
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
