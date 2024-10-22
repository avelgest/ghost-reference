#include "system_tray_icon.h"

#include <QtWidgets/QMenu>

#include "app.h"
#include "widgets/back_window.h"
#include "widgets/back_window_actions.h"
#include "widgets/main_toolbar.h"

namespace
{

    void showToolbar()
    {
        App *app = App::ghostRefInstance();
        if (app->mainToolbar())
        {
            app->mainToolbar()->show();
            app->setSystemTrayIconVisible(false);
        }
    }

    BackWindowActions *backWindowActions()
    {
        Q_ASSERT(App::ghostRefInstance()->backWindow());
        return App::ghostRefInstance()->backWindow()->backWindowActions();
    }

    void initContextMenu(QMenu *menu)
    {
        BackWindowActions *toolbarActions = backWindowActions();
        QAction *action = nullptr;

        action = menu->addAction("Restore Toolbar");
        QObject::connect(action, &QAction::triggered, menu, showToolbar);

        menu->addSeparator();
        action = menu->addAction("Open");
        QObject::connect(action, &QAction::triggered, []() { backWindowActions()->openSession().trigger(); });

        action = menu->addAction("Save");
        QObject::connect(action, &QAction::triggered, []() { backWindowActions()->saveSession().trigger(); });

        action = menu->addAction(toolbarActions->showPreferences().text());
        QObject::connect(action, &QAction::triggered, []() { backWindowActions()->showPreferences().trigger(); });

        menu->addSeparator();
        action = menu->addAction(toolbarActions->showHelp().text());
        QObject::connect(action, &QAction::triggered, []() { backWindowActions()->showHelp().trigger(); });

        menu->addSeparator();
        action = menu->addAction("Exit");
        QObject::connect(action, &QAction::triggered, menu, App::quit);
    }

} // namespace

SystemTrayIcon::SystemTrayIcon(QObject *parent)
    : QSystemTrayIcon(parent)
{
    initContextMenu(&m_contextMenu);

    setIcon(QIcon(":/appicon.ico"));
    setContextMenu(&m_contextMenu);
    // TODO Use BackWindow window title
    setToolTip(App::applicationName());

    QObject::connect(this, &QSystemTrayIcon::activated, this, &SystemTrayIcon::onActivated);
}

void SystemTrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case DoubleClick:
    case MiddleClick:
        showToolbar();
        break;

    default:
        break;
    }
}
