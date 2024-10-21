#include "main_toolbar_actions.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QStyle>
#include <QtWidgets/QSystemTrayIcon>

#include "../app.h"
#include "../reference_loading.h"
#include "../saving.h"
#include "../undo_stack.h"

#include "main_toolbar.h"
#include "preferences_window.h"

namespace
{
    App *getApp()
    {
        return App::ghostRefInstance();
    }

    void quitApplication()
    {
        App::quit();
    }

    void hideToolbarFnc()
    {
        if (QSystemTrayIcon::isSystemTrayAvailable())
        {
            App *app = App::ghostRefInstance();
            app->mainToolbar()->hide();
            app->setSystemTrayIconVisible(true);
        }
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
        App *const app = getApp();
        app->setGlobalMode((app->globalMode() == GhostMode) ? TransformMode : GhostMode);
    }

    void toggleAllRefsHiddenFnc()
    {
        App *app = getApp();
        app->setAllRefWindowsVisible(!app->allRefWindowsVisible());
    }

    QString appendShortcut(const QString &text, const QAction &action)
    {
        if (action.shortcut().isEmpty())
        {
            return text;
        }
        return text + " (" + action.shortcut().toString() + ')';
    }

} // namespace

MainToolbarActions::MainToolbarActions(MainToolbar *mainToolbar)
    : QObject(mainToolbar),
      m_windowModeGroup(nullptr)
{
    const App *const app = App::ghostRefInstance();
    const QClipboard *clipboard = App::clipboard();
    QStyle *style = mainToolbar->style();
    const bool darkMode = App::isDarkMode();

    static const QIcon icon_hidden(":/hidden.png");
    static const QIcon icon_visible(":/visible.png");

    // Close Application
    closeApplication().setIcon(QIcon(darkMode ? ":/app_quit_dark.png" : ":/app_quit.png"));
    closeApplication().setText("Quit");
    QObject::connect(&closeApplication(), &QAction::triggered, &quitApplication);

    // Toggle All Reference Windows Hidden
    toggleAllRefsHidden().setText("Hide/Show All");
    toggleAllRefsHidden().setCheckable(true);
    toggleAllRefsHidden().setChecked(!app->allRefWindowsVisible());
    toggleAllRefsHidden().setIcon(icon_visible);
    QObject::connect(app, &App::allRefWindowsVisibleChanged, this, [&](bool value) {
        toggleAllRefsHidden().setChecked(!value);
        toggleAllRefsHidden().setIcon(value ? icon_visible : icon_hidden);
    });
    QObject::connect(&toggleAllRefsHidden(), &QAction::triggered, &toggleAllRefsHiddenFnc);

    // Toggle Ghost Mode
    toggleGhostMode().setText("Ghost Mode");
    toggleGhostMode().setCheckable(true);
    toggleGhostMode().setChecked(app->globalMode() == GhostMode);
    toggleGhostMode().setIcon(QIcon(":/ghost_mode.png"));
    QObject::connect(app, &App::globalModeChanged,
                     &toggleGhostMode(), [this](WindowMode mode)
                     { toggleGhostMode().setChecked(mode == GhostMode); });
    QObject::connect(&toggleGhostMode(), &QAction::triggered, toggleGhostModeFnc);

    // Minimize Toolbar
    minimizeToolbar().setIcon(QIcon(darkMode ? ":/minimize_to_tray_dark.png" : ":/minimize_to_tray.png"));
    minimizeToolbar().setText("Minimize Toolbar to System Tray");
    QObject::connect(&minimizeToolbar(), &QAction::triggered, hideToolbarFnc);

    // Open
    openSession().setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    openSession().setShortcut(QKeySequence::Open);
    openSession().setText(appendShortcut("Open", openSession()));
    QObject::connect(&openSession(), &QAction::triggered, &openSessionFnc);

    // Save
    saveSession().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    saveSession().setShortcut(QKeySequence::Save);
    saveSession().setText(appendShortcut("Save", saveSession()));
    QObject::connect(&saveSession(), &QAction::triggered, &saveSessionFnc);

    // SaveAs
    saveSessionAs().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    saveSessionAs().setText("Save As");
    saveSessionAs().setShortcut(QKeySequence::SaveAs);
    QObject::connect(&saveSessionAs(), &QAction::triggered, []() { getApp()->saveSessionAs(); });

    // Paste
    paste().setEnabled(refLoad::isSupportedClipboard());
    paste().setText("Paste");
    QObject::connect(clipboard, &QClipboard::dataChanged, [this]()
                     { paste().setEnabled(refLoad::isSupportedClipboard()); });
    QObject::connect(&paste(), &QAction::triggered, [this]()
                     { MainToolbar::newReferenceWindow(refLoad::fromClipboard()); });

    // Show Help
    showHelp().setIcon(QIcon::fromTheme(QIcon::ThemeIcon::HelpFaq));
    showHelp().setText("Help");
    QObject::connect(&showHelp(), &QAction::triggered, &showHelpFnc);

    // Show Preferences
    showPreferences().setIcon(QIcon(":/preferences.png"));
    showPreferences().setText("Preferences");
    QObject::connect(&showPreferences(), &QAction::triggered, &showPreferencesFnc);

    // Undo
    undo().setShortcut(QKeySequence::Undo);
    undo().setText("Undo");
    QObject::connect(&undo(), &QAction::triggered, []() { getApp()->undoStack()->undo(); });

    // Redo
    redo().setShortcut(QKeySequence::Redo);
    redo().setText("Redo");
    QObject::connect(&redo(), &QAction::triggered, []() { getApp()->undoStack()->redo(); });

    m_windowModeGroup.addAction(&toggleGhostMode());
}

MainToolbar *MainToolbarActions::mainToolbar() const
{
    return qobject_cast<MainToolbar *>(parent());
}
