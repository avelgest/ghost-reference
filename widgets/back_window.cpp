#include "back_window.h"

#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

#include <QtWidgets/QApplication>

#include "../app.h"
#include "../utils/window_utils.h"
#include "back_window_actions.h"
#include "reference_window.h"
#include "settings_panel.h"

namespace
{
    const Qt::WindowFlags defaultWindowFlags = Qt::Window |
                                               Qt::FramelessWindowHint |
                                               Qt::WindowStaysOnTopHint |
                                               Qt::WindowMinimizeButtonHint | // For minimizing from the taskbar
                                               Qt::MaximizeUsingFullscreenGeometryHint;

    void addBackWindowActions(BackWindowActions *actions, QWidget *to)
    {
        // Add all actions with shortcuts
        QList<QAction *> to_add = actions->allActions();
        to_add.removeIf([](auto *x) { return x->shortcut().isEmpty(); });

        to->addActions(to_add);
    }

} // namespace

BackWindow::BackWindow(QWidget *parent)
    : QWidget(parent, defaultWindowFlags),
      m_backWindowActions(new BackWindowActions(this))
{
    setAttribute(Qt::WA_TranslucentBackground);

    setGeometry(screen()->virtualGeometry());

    addBackWindowActions(m_backWindowActions, this);

    QObject::connect(App::ghostRefInstance(), &App::windowModeChanged, this, &BackWindow::setWindowMode);

#ifndef Q_OS_WIN32 // Windows uses the icon in ghost_reference.rc
    setWindowIcon(QIcon(":/appicon.ico"));
#endif // !WIN32
}

SettingsPanel *BackWindow::settingsWindow()
{
    return m_settingsPanel;
}

SettingsPanel *BackWindow::showSettingsWindow(const QPoint &atPos)
{
    const int vOffset = -100;

    if (m_settingsPanel.isNull())
    {
        m_settingsPanel = new SettingsPanel(ReferenceWindow::activeWindow(), this);
        m_settingsPanel->setAttribute(Qt::WA_DeleteOnClose);
        m_settingsPanel->show();
    }
    QPoint adjustedPos = atPos.isNull() ? QCursor::pos() : atPos;
    adjustedPos += QPoint(-m_settingsPanel->width() / 2, vOffset);

    m_settingsPanel->move(adjustedPos);
    m_settingsPanel->show();

    m_settingsPanel->raise();
    m_settingsPanel->setFocus();
    return m_settingsPanel;
}

void BackWindow::hideSettingsWindow()
{
    if (!m_settingsPanel.isNull())
    {
        m_settingsPanel->hide();
    }
}

void BackWindow::setWindowMode(WindowMode value)
{
    m_windowMode = value;
    hideSettingsWindow();
    utils::setTransparentForInput(this, value == GhostMode);
}

void BackWindow::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);
    const int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || Qt::Key_Alt)
    {
        emit modifierKeysChanged(event->modifiers());
    }
}

void BackWindow::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
    const int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || Qt::Key_Alt)
    {
        emit modifierKeysChanged(event->modifiers());
    }
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
