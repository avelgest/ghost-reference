#include "main_window_actions.h"

#include <QtWidgets/QStyle>

#include "../app.h"
#include "main_window.h"

void toggleGhostModeFnc()
{
    App *const app = App::ghostRefInstance();
    app->setGlobalMode((app->globalMode() == GhostMode) ? TransformMode : GhostMode);
}

Actions::Actions(MainWindow *mainWindow)
    : QObject(mainWindow)
{
    const App *const app = App::ghostRefInstance();
    QStyle *style = mainWindow->style();

    closeApplication().setIcon(style->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeApplication().setText("Exit");
    QObject::connect(&closeApplication(), &QAction::triggered, mainWindow, &MainWindow::close);

    toggleGhostMode().setText("Ghost Mode");
    toggleGhostMode().setCheckable(true);
    toggleGhostMode().setChecked(app->globalMode() == GhostMode);
    QObject::connect(app, &App::globalModeOverrideChanged,
                     &toggleGhostMode(), [this](std::optional<WindowMode> mode)
                     { this->toggleGhostMode().setChecked(mode == GhostMode); });
    QObject::connect(&toggleGhostMode(), &QAction::triggered, toggleGhostModeFnc);

    minimizeApplication().setIcon(style->standardIcon(QStyle::SP_TitleBarMinButton));
    minimizeApplication().setText("Minimize All");
    QObject::connect(&minimizeApplication(), &QAction::triggered, mainWindow, &MainWindow::showMinimized);

    open().setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    open().setText("Open");

    save().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    save().setText("Save");
}

MainWindow *Actions::mainWindow() const
{
    return qobject_cast<MainWindow *>(parent());
}