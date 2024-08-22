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
    App *getApp()
    {
        return App::ghostRefInstance();
    }

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

    struct
    {
    private:
        friend class ::MainToolbarActions;
        bool m_initialized = false;

        QIcon hidden, preferences, visible;

        void init()
        {
            if (!m_initialized)
            {
                hidden = QIcon(":/hidden.png");
                preferences = QIcon(":/preferences.png");
                visible = QIcon(":/visible.png");
            }
            m_initialized = true;
        }

    } iconCache;

} // namespace

MainToolbarActions::MainToolbarActions(MainToolbar *mainToolbar)
    : QObject(mainToolbar),
      m_windowModeGroup(nullptr)
{
    const App *const app = App::ghostRefInstance();
    const QClipboard *clipboard = App::clipboard();
    QStyle *style = mainToolbar->style();
    iconCache.init();

    // Close Application
    closeApplication().setIcon(style->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeApplication().setText("Exit");
    QObject::connect(&closeApplication(), &QAction::triggered, &quitApplication);

    // Toggle All Reference Windows Hidden
    toggleAllRefsHidden().setText("Hide/Show All");
    toggleAllRefsHidden().setCheckable(true);
    toggleAllRefsHidden().setChecked(!app->allRefWindowsVisible());
    toggleAllRefsHidden().setIcon(iconCache.visible);
    QObject::connect(app, &App::allRefWindowsVisibleChanged, this, [this](bool value) {
        toggleAllRefsHidden().setChecked(!value);
        toggleAllRefsHidden().setIcon(value ? iconCache.visible : iconCache.hidden);
    });
    QObject::connect(&toggleAllRefsHidden(), &QAction::triggered, &toggleAllRefsHiddenFnc);

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
    openSession().setShortcut(QKeySequence::Open);
    QObject::connect(&openSession(), &QAction::triggered, &openSessionFnc);

    // Save
    saveSession().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    // TODO Add actions that have shorcuts to app.back_window
    saveSession().setShortcut(QKeySequence::Save);
    saveSession().setText(appendShortcut("Save", saveSession()));
    saveSession().setShortcut(QKeySequence::Save);
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
    showHelp().setIcon(style->standardIcon(QStyle::SP_TitleBarContextHelpButton));
    showHelp().setText("Help");
    QObject::connect(&showHelp(), &QAction::triggered, &showHelpFnc);

    // Show Preferences
    showPreferences().setIcon(iconCache.preferences);
    showPreferences().setText("Preferences");
    QObject::connect(&showPreferences(), &QAction::triggered, &showPreferencesFnc);

    m_windowModeGroup.addAction(&toggleGhostMode());
}

MainToolbar *MainToolbarActions::mainToolbar() const
{
    return qobject_cast<MainToolbar *>(parent());
}
