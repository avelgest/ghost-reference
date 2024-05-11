#include "window_utils.h"

#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif // Q_OS_WIN

#if defined(Q_OS_WIN)
void utils::setTransparentForInput(QWidget *windowWidget, bool value)
{
    QWindow *window = windowWidget->windowHandle();
    if (window)
    {
        HWND handle = reinterpret_cast<HWND>(window->winId());
        const LONG styles = GetWindowLongA(handle, GWL_EXSTYLE);
        const LONG newStyles = value ? styles | WS_EX_TRANSPARENT : styles & ~WS_EX_TRANSPARENT;

        if (newStyles != styles)
        {
            if (const LONG err = SetWindowLongA(handle, GWL_EXSTYLE, newStyles); err != styles)
            {
                qCritical() << "Error" << err << "when setting WS_EX_TRANSPARENT window style";
            }
        }
    }
}

#else
void utils::setTransparentForInput(QWidget *windowWidget, bool value)
{
    QWindow *windowHandle = (windowWidget) ? windowWidget->windowHandle() : nullptr;

    if (windowHandle)
    {
        auto flags = windowHandle->flags();
        if (flags.testAnyFlags(Qt::WindowTransparentForInput) == value)
        {
            return;
        }
        flags.setFlag(Qt::WindowTransparentForInput, value);

        windowHandle->setFlags(flags);
        windowHandle->show();
        windowHandle->requestUpdate();
    }
}
#endif // Q_OS_WIN