#include "back_window_actions.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QClipboard>

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyle>
#include <QtWidgets/QSystemTrayIcon>

#include "../app.h"
#include "../reference_loading.h"
#include "../saving.h"
#include "../undo_stack.h"

#include "../tools/color_picker.h"
#include "../tools/extract_tool.h"

#include "back_window.h"
#include "help_window.h"
#include "main_toolbar.h"
#include "preferences_window.h"
#include "reference_window.h"

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

    ReferenceWindow *loadReference(const QString &filepath)
    {
        if (const ReferenceImageSP refImage = refLoad::fromFilepath(filepath); refImage)
        {
            ReferenceWindow *refWin = getApp()->newReferenceWindow();
            refWin->addReference(refImage);
            refWin->show();
            return refWin;
        }

        QMessageBox msgBox(QMessageBox::Warning, "Unable to Load Reference",
                           "Unable to load reference from " + filepath, QMessageBox::Ok);
        getApp()->initMsgBox(msgBox);
        msgBox.exec();

        return nullptr;
    }

    QString getOpenDirectoryPath()
    {
        if (const QString &sessPath = getApp()->saveFilePath(); !sessPath.isEmpty())
        {
            const QFileInfo fileInfo(sessPath);
            return fileInfo.dir().absolutePath();
        }
        return "";
    }

    void toggleToolbarFnc()
    {
        if (QSystemTrayIcon::isSystemTrayAvailable())
        {
            App *app = getApp();
            if (MainToolbar *toolbar = app->mainToolbar(); toolbar)
            {
                toolbar->setVisible(!toolbar->isVisible());
                app->setSystemTrayIconVisible(!toolbar->isVisible());
            }
        }
    }

    void openAnyFnc()
    {
        const QString filepath = sessionSaving::showOpenDialog(getOpenDirectoryPath(), true, true);
        if (!filepath.isEmpty())
        {
            if (sessionSaving::isSessionFile(filepath))
            {
                getApp()->loadSession(filepath);
            }
            else
            {
                loadReference(filepath);
            }
        }
    }

    void openReferenceFnc()
    {
        const QString filepath = sessionSaving::showOpenDialog(getOpenDirectoryPath(), false, true);
        if (!filepath.isEmpty())
        {
            loadReference(filepath);
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
        auto *helpWindow = new HelpWindow(getApp()->backWindow());
        helpWindow->show();
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

} // namespace

QList<QAction *> BackWindowActions::allActions()
{
    return {&closeApplication(), &colorPicker(), &extractTool(), &newSession(),          &openAny(),
            &openReference(),    &openSession(), &paste(),       &toggleAllRefsHidden(), &toggleGhostMode(),
            &toggleToolbar(),    &redo(),        &saveSession(), &saveSessionAs(),       &showHelp(),
            &showPreferences(),  &undo()};
}

BackWindowActions::BackWindowActions(BackWindow *backWindow)
    : QObject(backWindow),
      m_windowModeGroup(nullptr)
{
    const App *const app = App::ghostRefInstance();
    const QClipboard *clipboard = App::clipboard();
    QStyle *style = backWindow->style();
    const bool darkMode = App::isDarkMode();

    static const QIcon icon_hidden(":/hidden.png");
    static const QIcon icon_visible(":/visible.png");

    // Close Application
    closeApplication().setIcon(QIcon(darkMode ? ":/app_quit_dark.png" : ":/app_quit.png"));
    closeApplication().setText("Quit");
    QObject::connect(&closeApplication(), &QAction::triggered, &quitApplication);

    // Color Picker
    colorPicker().setIcon(QIcon(darkMode ? ":/color_picker_dark.png" : ":/color_picker.png"));
    colorPicker().setText("Color Picker");
    colorPicker().setShortcut(Qt::Key_C);
    QObject::connect(&colorPicker(), &QAction::triggered, []() { Tool::activateTool<ColorPicker>(); });

    // Extract Tool
    extractTool().setIcon(QIcon(":/extract_tool.png"));
    extractTool().setText("Extract to New Window");
    extractTool().setShortcut(Qt::Key_E);
    extractTool().setToolTip(
        "Extract - Select an area of a reference image with the mouse to open that area in a new window.");
    QObject::connect(&extractTool(), &QAction::triggered, []() { Tool::activateTool<ExtractTool>(); });

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
    QObject::connect(app, &App::globalModeChanged, &toggleGhostMode(),
                     [this]() { toggleGhostMode().setChecked(getApp()->globalMode() == GhostMode); });
    QObject::connect(&toggleGhostMode(), &QAction::triggered, toggleGhostModeFnc);

    // Minimize Toolbar
    toggleToolbar().setIcon(QIcon(darkMode ? ":/minimize_to_tray_dark.png" : ":/minimize_to_tray.png"));
    toggleToolbar().setText("Minimize Toolbar to System Tray");
    toggleToolbar().setShortcut(Qt::CTRL | Qt::Key_M);
    QObject::connect(&toggleToolbar(), &QAction::triggered, toggleToolbarFnc);

    // New Session
    newSession().setIcon(style->standardIcon(QStyle::SP_FileIcon));
    newSession().setText("New Session");
    QObject::connect(&newSession(), &QAction::triggered, []() { getApp()->newSession(); });

    // Open Any
    openAny().setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    openAny().setText("Open");
    openAny().setShortcut(QKeySequence::Open);
    QObject::connect(&openAny(), &QAction::triggered, &openAnyFnc);

    // Open Reference
    openReference().setIcon(openAny().icon());
    openReference().setText("Open Reference Image");
    QObject::connect(&openReference(), &QAction::triggered, &openReferenceFnc);

    // Open Session
    openSession().setIcon(openAny().icon());
    openSession().setText("Open Session");
    QObject::connect(&openSession(), &QAction::triggered, &openSessionFnc);

    // Save
    saveSession().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    saveSession().setShortcut(QKeySequence::Save);
    saveSession().setText("Save");
    QObject::connect(&saveSession(), &QAction::triggered, &saveSessionFnc);

    // SaveAs
    saveSessionAs().setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    saveSessionAs().setText("Save As");
    saveSessionAs().setShortcut(QKeySequence::SaveAs);
    QObject::connect(&saveSessionAs(), &QAction::triggered, []() { getApp()->saveSessionAs(); });

    // Paste
    paste().setEnabled(refLoad::isSupportedClipboard());
    paste().setText("Paste");
    paste().setShortcut(QKeySequence::Paste);
    paste().setShortcutContext(Qt::ShortcutContext::ApplicationShortcut);
    QObject::connect(clipboard, &QClipboard::dataChanged,
                     [this]() { paste().setEnabled(refLoad::isSupportedClipboard()); });
    QObject::connect(&paste(), &QAction::triggered,
                     []() { refLoad::pasteRefsFromClipboard(ReferenceWindow::activeWindow()); });

    // Show Help
    showHelp().setIcon(QIcon::fromTheme(QIcon::ThemeIcon::HelpFaq));
    showHelp().setText("Help");
    showHelp().setShortcut(QKeySequence::HelpContents);
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

    for (auto *action : allActions())
    {
        if (!action->shortcut().isEmpty())
        {
            const QString currentTooltip = (action->toolTip().isEmpty() ? action->text() : action->toolTip()) + " (%0)";
            action->setToolTip(currentTooltip.arg(action->shortcut().toString(QKeySequence::NativeText)));
        }
    }
}

BackWindow *BackWindowActions::backWindow() const
{
    return qobject_cast<BackWindow *>(parent());
}
