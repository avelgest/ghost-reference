#include "app.h"

#include <set>

#include <QtCore/QCommandLineParser>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>
#include <QtCore/QRunnable>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>

#include <QtGui/QCursor>
#include <QtGui/QScreen>
#include <QtGui/QStyleHints>
#include <QtNetwork/QNetworkAccessManager>
#include <QtWidgets/QMessageBox>

#include "global_hotkeys.h"
#include "logger.h"
#include "preferences.h"
#include "reference_collection.h"
#include "reference_image.h"
#include "reference_loading.h"
#include "saving.h"
#include "system_tray_icon.h"
#include "undo_stack.h"

#include "tools/tool.h"

#include "widgets/back_window.h"
#include "widgets/main_toolbar.h"
#include "widgets/reference_window.h"

namespace
{
    const qreal timerCallsPerSecond = 24.0;
    const WindowMode defaultWindowMode = TransformMode;
    const char *const styleSheetPath = ":/stylesheet.qss";
    const char *const styleSheetDarkPath = ":/stylesheet_dark.qss";

    const int timerIntervalMs = static_cast<int>(1000.0 / timerCallsPerSecond);

    const Qt::WindowFlags msgBoxWindowFlags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint;

    struct CmdParseResult
    {
        QString sessionFile;
        QStringList references;
    };

    CmdParseResult parseCommandLine(const App *app)
    {
        QCommandLineParser parser;
        parser.addPositionalArgument("session", "Session file (.ghr) to open when the application starts.");
        parser.addOption({{"r", "ref"}, "Open <file> as a reference image.", "file"});
        parser.addHelpOption();
        parser.process(*app);

        CmdParseResult result;
        QStringList positional = parser.positionalArguments();

        if (positional.size() == 1)
        {
            result.sessionFile = positional.first();
        }
        else if (!positional.isEmpty())
        {
            qCritical() << "Expected only one positional argument.";
        }
        result.references = parser.values("ref");
        return result;
    }

    void loadStyleSheetFor(App *app, bool replace = false)
    {
        QString styleSheetString;

        QFile styleSheetFile(styleSheetPath);

        if (styleSheetFile.open(QFile::ReadOnly))
        {
            styleSheetString.append(QTextStream(&styleSheetFile).readAll());
        }
        else
        {
            qWarning() << "Unable to load style sheet " << styleSheetPath;
        }

        // Append the style sheet for dark mode
        if (App::isDarkMode())
        {
            QFile styleSheetFileDark(styleSheetDarkPath);
            if (styleSheetFileDark.open(QFile::ReadOnly))
            {
                styleSheetString.append(QTextStream(&styleSheetFileDark).readAll());
            }
            else
            {
                qWarning() << "Unable to load style sheet " << styleSheetDarkPath;
            }
        }

        if (!replace)
        {
            styleSheetString.append(app->styleSheet());
        }

        app->setStyleSheet(styleSheetString);
    }

    void positionToolBarDefault(QWidget *toolbar)
    {
        QScreen *screen = toolbar->screen();
        QPoint moveTo(0, 0);
        if (screen)
        {
            moveTo = {screen->size().width() / 4, screen->size().height() / 4};
        }
        toolbar->move(moveTo);
    }

    int showUnsavedChangesMsgBox(const App *app)
    {
        QMessageBox msgBox;
        app->initMsgBox(msgBox);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.setWindowTitle("Ghost Reference");

        if (app->saveFilePath().isEmpty())
        {
            msgBox.setText("Save session?");
        }
        else
        {
            msgBox.setText("Save changes to " + app->saveFilePath() + '?');
        }

        return msgBox.exec();
    }

    // Displays a messagebox asking the user about unsaved changes (if necessary) and saves the
    // session if requested.
    // Returns false if the user requested to remain in the current session (e.g. by pressing Cancel
    // in the messagebox) or true overwise;
    bool askUnsavedChangesOk(App *app)
    {
        if (app->hasUnsavedChanges() && app->preferences()->getBool(Preferences::AskSaveBeforeClosing))
        {
            switch (showUnsavedChangesMsgBox(app))
            {
            case QMessageBox::Cancel:
                return false;
            case QMessageBox::Save:
                return app->saveSession();
            default:
                return true;
            }
        }
        return true;
    }

    int showSaveErrorMsgBox(const App *app)
    {
        QMessageBox msgBox(QMessageBox::Warning, "Error Saving Session",
                           "Unable to save session to " + app->saveFilePath(), QMessageBox::NoButton);
        app->initMsgBox(msgBox);

        return msgBox.exec();
    }

    // QMimeDatabase may take a long time to load.
    // Use this function to load it in a separate thread so that the application doesn't hang.
    void preloadMimeDatabase()
    {
        class PreloadMimeDatabase : public QRunnable
        {
        public:
            void run() override
            {
                const QMimeDatabase database;
                database.mimeTypesForFileName("dummy.jpg");
            }
        };

        QThreadPool::globalInstance()->start(new PreloadMimeDatabase(), QThread::LowPriority);
    }

    RefWindowId createRefWindowId()
    {
        static RefWindowId previous = 0;
        return ++previous;
    }

} // namespace

void App::checkModifierKeyStates()
{
    const bool overrideKeyHeld = isOverrideKeyHeld();
    if (overrideKeyHeld)
    {
        if (!inOverrideMode())
        {
            startGlobalModeOverride(WindowMode::TransformMode);
        }
    }
    else if (inOverrideMode())
    {
        endGlobalModeOverride();
    }
}

void App::checkGhostStates()
{
    // N.B. QCursor::pos is relative to the primary screen
    const QPoint cursor_pos = m_backWindow->mapFromGlobal(QCursor::pos());

    for (auto &refWindow : m_refWindows)
    {
        if (!refWindow || !refWindow->isVisible())
        {
            continue;
        }
        // TODO Move code to ReferenceWindow
        if (refWindow->windowMode() == WindowMode::GhostMode)
        {
            const bool isMouseOver = refWindow->geometry().contains(cursor_pos);

            if (isMouseOver && !refWindow->ghostState())
            {
                refWindow->setGhostState(true);
            }
            else if (!isMouseOver && refWindow->ghostState())
            {
                refWindow->setGhostState(false);
            }
        }
    }
}

bool App::isOverrideKeyHeld()
{
    return m_overrideKeys != Qt::NoModifier && queryKeyboardModifiers().testFlags(m_overrideKeys);
}

void App::setGlobalMode(WindowMode mode)
{
    m_globalMode = mode;
    emit globalModeChanged(m_globalMode);

    if (!inOverrideMode())
    {
        emit windowModeChanged(mode);
    }

    if (mode == WindowMode::GhostMode && Tool::activeTool())
    {
        Tool::activeTool()->deactivate();
    }

    // Start/stop the checkGhostStates timer
    if (mode == WindowMode::GhostMode)
    {
        if (m_timer == 0)
        {
            m_timer = startTimer(timerIntervalMs);
        }
    }
    else if (m_timer != 0)
    {
        killTimer(m_timer);
        m_timer = 0;
    }
}

QNetworkAccessManager *App::networkManager()
{
    if (!m_networkManager)
    {
        m_networkManager = new QNetworkAccessManager(this);
    }
    return m_networkManager;
}

void App::setPreferences(Preferences *prefs)
{
    if (prefs == nullptr)
    {
        qCritical() << "App::setPreferences called with null pointer";
        return;
    }

    prefs->setParent(this);
    m_preferences = prefs;
    m_overrideKeys = prefs->overrideKeys();
    emit preferencesReplaced(prefs);
}

ReferenceWindow *App::newReferenceWindow()
{
    if (!backWindow())
    {
        throw std::runtime_error("Cannot create a ReferenceWindow without a BackWindow.");
    }
    auto *refWindow = new ReferenceWindow(backWindow());
    refWindow->setIdentifier(createRefWindowId());

    QObject::connect(refWindow, &ReferenceWindow::destroyed, this,
                     [this](QObject *ptr) { m_refWindows.removeAll(ptr); });

    m_refWindows.push_back(refWindow);
    emit referenceWindowAdded(refWindow);

    return refWindow;
}

ReferenceWindow *App::getReferenceWindow(RefWindowId identifier) const
{
    for (const auto &refWinPtr : m_refWindows)
    {
        if (refWinPtr && refWinPtr->identifier() == identifier)
        {
            return refWinPtr;
        }
    }
    return nullptr;
}

void App::startGlobalModeOverride(std::optional<WindowMode> windowMode)
{
    const WindowMode unwrapped = windowMode.value_or(m_globalMode);

    m_globalModeOverride = windowMode;

    emit windowModeChanged(unwrapped);
}

void App::endGlobalModeOverride()
{
    startGlobalModeOverride(std::nullopt);
}

void App::setReferenceCursor(const std::optional<QCursor> &cursor, std::optional<RefType> refType)
{
    // The actual changing of cursors should be done by widgets themselves after connecting
    // to the referenceCursorChanged signal.
    emit referenceCursorChanged(cursor, refType);
}

bool App::saveSession()
{
    if (m_saveFilePath.isEmpty())
    {
        QString filePath = sessionSaving::showSaveAsDialog();
        if (filePath.isEmpty())
        {
            return false;
        }
        m_saveFilePath = std::move(filePath);
    }
    if (!sessionSaving::saveSession(m_saveFilePath))
    {
        showSaveErrorMsgBox(this);
        return false;
    }
    m_hasUnsavedChanges = false;
    refreshWindowName();
    return true;
}

bool App::saveSessionAs()
{
    const QString filePath = sessionSaving::showSaveAsDialog(m_saveFilePath);
    if (filePath.isEmpty())
    {
        return false;
    }
    if (!sessionSaving::saveSession(m_saveFilePath))
    {
        showSaveErrorMsgBox(this);
        return false;
    }
    return true;
}

void App::loadSession()
{
    const QString filepath = sessionSaving::showOpenDialog(m_saveFilePath, true, false);
    if (filepath.isEmpty())
    {
        return;
    }
    loadSession(filepath);
}

void App::loadSession(const QString &filepath)
{
    // Ask user about unsaved changes
    if (!askUnsavedChangesOk(this))
    {
        return;
    }
    if (sessionSaving::loadSession(filepath))
    {
        m_saveFilePath = filepath;
        m_hasUnsavedChanges = false;
        refreshWindowName();
    }
    else
    {
        qCritical() << "Unable to load session from " << filepath;
    }
}

void App::newSession(bool force)
{
    // Ask user about unsaved changes
    if (!force && !askUnsavedChangesOk(this))
    {
        return;
    }

    setGlobalMode(TransformMode);
    m_globalModeOverride = {};

    closeAllReferenceWindows();
    m_referenceItems->clear();
    m_undoStack->clear();
    m_saveFilePath.clear();
    setUnsavedChanges(false);
    refreshWindowName();
}

void App::setUnsavedChanges(bool value)
{
    if (value != m_hasUnsavedChanges)
    {
        m_hasUnsavedChanges = value;
        refreshWindowName();
    }
}

void App::setAllRefWindowsVisible(bool value)
{
    for (const auto &refWindow : m_refWindows)
    {
        if (!refWindow->ghostRefHidden())
        {
            refWindow->setVisible(value);
        }
    }
    if (m_allRefWindowsVisible != value)
    {
        m_allRefWindowsVisible = value;
        emit allRefWindowsVisibleChanged(value);
    }
}

void App::closeAllReferenceWindows()
{
    for (const auto &refWindow : m_refWindows)
    {
        refWindow->deleteLater();
    }
    m_refWindows.clear();
}

void App::setSystemTrayIconVisible(bool value)
{
    if (value && !m_systemTrayIcon)
    {
        m_systemTrayIcon = new SystemTrayIcon(this);
    }
    if (m_systemTrayIcon)
    {
        m_systemTrayIcon->setVisible(value);
    }
}

// TODO Move to utils
void App::initMsgBox(QMessageBox &msgBox) const
{
    msgBox.setParent(m_backWindow);
    msgBox.setWindowFlags(msgBoxWindowFlags);
}

void App::onStartUp()
{
    processCommandLineArgs();

    // Create an empty reference window if none have been added
    if (m_refWindows.isEmpty())
    {
        ReferenceWindow *refWindow = newReferenceWindow();
        refWindow->show();
    }
    setGlobalMode(defaultWindowMode);
}

bool App::isDarkMode()
{
    return App::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

bool App::event(QEvent *event)
{
    // Ask to save if exiting when ther are unsaved changes
    if (event->type() == QEvent::Quit && hasUnsavedChanges() &&
        preferences()->getBool(Preferences::AskSaveBeforeClosing))
    {
        if (!askUnsavedChangesOk(this))
        {
            return true;
        }
    }
    return QApplication::event(event);
}

void App::timerEvent([[maybe_unused]] QTimerEvent *event)
{
    checkGhostStates();
    checkModifierKeyStates();
}

void App::cleanWindowList()
{
    m_refWindows.removeIf([](auto &refWindow)
                          { return !refWindow; });
}

void App::processCommandLineArgs()
{
    const CmdParseResult parsed = parseCommandLine(this);
    if (!parsed.sessionFile.isEmpty())
    {
        this->loadSession(parsed.sessionFile);
    }

    // Load any specified reference items
    for (const auto &filename : parsed.references)
    {
        const ReferenceImageSP refItem = refLoad::fromUrl(QUrl::fromUserInput(filename));
        if (refItem && refItem->isValid())
        {
            ReferenceWindow *refWindow = newReferenceWindow();
            refWindow->addReference(refItem, true);
            refWindow->show();
        }
    }
}

void App::refreshWindowName()
{
    const QString baseName = applicationDisplayName();
    if (saveFilePath().isEmpty())
    {
        m_backWindow->setWindowTitle(baseName);
    }
    else
    {
        const QString filename = QFileInfo(saveFilePath()).fileName();
        QString appName = filename + " - " + baseName;
        if (hasUnsavedChanges())
        {
            appName.prepend('*');
        }
        m_backWindow->setWindowTitle(appName);
    }
}

App::App(int &argc, char **argv, int flags, const Preferences *prefs)
    : QApplication(argc, argv, flags),
      m_globalMode(defaultWindowMode),
      m_logger(new Logger(this)),
      m_referenceItems(std::make_unique<ReferenceCollection>())
{
    setApplicationName("Ghost Reference");
    setApplicationDisplayName("Ghost Reference");

    // Loading preferences from disk requires applicationName to be set first. So initialize everything here
    // instead of as member initailizers.
    setPreferences(prefs ? prefs->duplicate(this) : Preferences::loadFromDisk(this));

    m_logger->removeOldLogFiles();

    m_backWindow = new BackWindow();
    m_globalHotkeys = new GlobalHotkeys(this);
    m_mainToolbar = new MainToolbar(m_backWindow);
    m_undoStack = new UndoStack(this);

    refreshWindowName();
    loadStyleSheetFor(this);
    preloadMimeDatabase();

    m_backWindow->show();

    positionToolBarDefault(m_mainToolbar);
    m_mainToolbar->show();
}

App::~App() = default;
