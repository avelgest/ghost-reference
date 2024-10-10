#include "system_tray_icon.h"

#include <QtWidgets/QMenu>

#include "app.h"
#include "widgets/back_window.h"
#include "widgets/main_toolbar.h"
#include "widgets/main_toolbar_actions.h"

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

    MainToolbarActions *mainToolbarActions()
    {
        Q_ASSERT(App::ghostRefInstance()->backWindow());
        return App::ghostRefInstance()->backWindow()->mainToolbarActions();
    }

    void initContextMenu(QMenu *menu)
    {
        MainToolbarActions *toolbarActions = mainToolbarActions();
        QAction *action = nullptr;

        action = menu->addAction("Restore Toolbar");
        QObject::connect(action, &QAction::triggered, menu, showToolbar);

        menu->addSeparator();
        action = menu->addAction("Open");
        QObject::connect(action, &QAction::triggered, []() { mainToolbarActions()->openSession().trigger(); });

        action = menu->addAction("Save");
        QObject::connect(action, &QAction::triggered, []() { mainToolbarActions()->saveSession().trigger(); });

        action = menu->addAction(toolbarActions->showPreferences().text());
        QObject::connect(action, &QAction::triggered, []() { mainToolbarActions()->showPreferences().trigger(); });

        menu->addSeparator();
        action = menu->addAction(toolbarActions->showHelp().text());
        QObject::connect(action, &QAction::triggered, []() { mainToolbarActions()->showHelp().trigger(); });

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
