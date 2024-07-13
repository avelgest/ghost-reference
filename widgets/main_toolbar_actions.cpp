#include "main_toolbar_actions.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QStyle>

#include "../app.h"
#include "../reference_loading.h"
#include "../saving.h"

#include "main_toolbar.h"
#include "preferences_window.h"

namespace
{
    void quitApplication()
    {
        App::quit();
    }

    void minimizeAllWindows()
    {
        App::ghostRefInstance()->backWindow()->showMinimized();
    }

    void openSessionFnc()
    {
        App::ghostRefInstance()->loadSession();
    }

    void saveSessionFnc()
    {
        App::ghostRefInstance()->saveSession();
    }

    void showHelpFnc()
    {
    }

    void showPreferencesFnc()
    {
        auto *prefWindow = new PreferencesWindow(App::ghostRefInstance()->backWindow());
        prefWindow->show();
    }

    void toggleGhostModeFnc()
    {
        App *const app = App::ghostRefInstance();
        app->setGlobalMode((app->globalMode() == GhostMode) ? TransformMode : GhostMode);
    }

    App *getApp()
    {
        return App::ghostRefInstance();
    }
} // namespace

MainToolbarActions::MainToolbarActions(MainToolbar *mainToolbar)
    : QObject(mainToolbar),
      m_windowModeGroup(nullptr)
{
    const App *const app = App::ghostRefInstance();
    const QClipboard *clipboard = App::clipboard();
    QStyle *style = mainToolbar->style();

    // Close Application
    closeApplication().setIcon(style->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeApplication().setText("Exit");
    QObject::connect(&closeApplication(), &QAction::triggered, &quitApplication);

    // Toggle Ghost Mode
    toggleGhostMode().setText("Ghost Mode");
    toggleGhostMode().setCheckable(true);
    toggleGhostMode().setChecked(app->globalMode() == GhostMode);
    QObject::connect(app, &App::globalModeChanged,
                     &toggleGhostMode(), [this](WindowMode mode)
                     { toggleGhostMode().setChecked(mode == GhostMode); });
    QObject::connect(&toggleGhostMode(), &QAction::triggered, toggleGhostModeFnc);

    // Minimize Application
    minimizeApplication().setIcon(style->standardIcon(QStyle::SP_TitleBarMinButton));
    minimizeApplication().setText("Minimize All");
    QObject::connect(&minimizeApplication(), &QAction::triggered, &minimizeAllWindows);

    // Open
    openSession().setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    openSession().setText("Open");
    QObject::connect(&openSession(), &QAction::triggered, &openSessionFnc);

    // Save
    saveSession().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    saveSession().setText("Save");
    QObject::connect(&saveSession(), &QAction::triggered, &saveSessionFnc);

    // SaveAs
    saveSessionAs().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    saveSessionAs().setText("Save As");
    QObject::connect(&saveSessionAs(), &QAction::triggered, []() { getApp()->saveSessionAs(); });

    // Paste
    paste().setEnabled(refLoad::isSupportedClipboard());
    paste().setText("Paste");
    QObject::connect(clipboard, &QClipboard::dataChanged, [this]()
                     { paste().setEnabled(refLoad::isSupportedClipboard()); });
    QObject::connect(&paste(), &QAction::triggered, [this]()
                     { MainToolbar::newReferenceWindow(refLoad::fromClipboard()); });

    // Show Help
    showHelp().setIcon(style->standardIcon(QStyle::SP_TitleBarContextHelpButton));
    showHelp().setText("Help");
    QObject::connect(&showHelp(), &QAction::triggered, &showHelpFnc);

    // Show Preferences
    showPreferences().setText("Preferences");
    QObject::connect(&showPreferences(), &QAction::triggered, &showPreferencesFnc);

    m_windowModeGroup.addAction(&toggleGhostMode());
}

MainToolbar *MainToolbarActions::mainToolbar() const
{
    return qobject_cast<MainToolbar *>(parent());
}
