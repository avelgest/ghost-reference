#include "app.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QTextStream>

#include <QtGui/QCursor>
#include <QtGui/QScreen>
#include <QtNetwork/QNetworkAccessManager>

#include "preferences.h"
#include "saving.h"

#include "widgets/back_window.h"
#include "widgets/main_toolbar.h"
#include "widgets/reference_window.h"

namespace
{
    const qreal timerCallsPerSecond = 24.0;
    const WindowMode defaultWindowMode = TransformMode;
    const char *const styleSheetPath = "./resources/stylesheet.qss";

    struct CmdParseResult
    {
        QStringList files;
    };

    CmdParseResult parseCommandLine(const App *app)
    {
        QCommandLineParser parser;
        parser.addPositionalArgument("files", "Session or image files to open when the application starts.",
                                     "[files...]");
        parser.process(*app);

        return {parser.positionalArguments()};
    }

    void loadStyleSheetFor(QApplication *app, bool replace = false)
    {
        if (!replace && !app->styleSheet().isEmpty())
        {
            return;
        }

        QFile styleSheetFile(styleSheetPath);

        if (styleSheetFile.open(QFile::ReadOnly))
        {
            QTextStream textStream(&styleSheetFile);
            app->setStyleSheet(textStream.readAll());
        }
        else
        {
            qCritical() << "Unable to load style sheet" << styleSheetPath;
        }
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
    const QPoint cursor_pos = QCursor::pos();

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
    return queryKeyboardModifiers() & Qt::AltModifier;
}

void App::setGlobalMode(WindowMode mode)
{
    m_globalMode = mode;
    if (!inOverrideMode())
    {
        m_backWindow->setWindowMode(mode);
        emit globalModeChanged(mode);
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

ReferenceWindow *App::newReferenceWindow()
{
    if (!backWindow())
    {
        throw std::runtime_error("Cannot create a ReferenceWindow without a BackWindow.");
    }
    auto *refWindow = new ReferenceWindow(backWindow());
    refWindow->setWindowMode(m_globalMode);
    QObject::connect(this, &App::globalModeChanged, refWindow, &ReferenceWindow::setWindowMode);
    QObject::connect(refWindow, &ReferenceWindow::destroyed, [this](QObject *ptr)
                     { m_refWindows.removeAll(ptr); });

    m_refWindows.push_back(refWindow);

    return refWindow;
}

void App::startGlobalModeOverride(std::optional<WindowMode> windowMode)
{
    const WindowMode unwrapped = windowMode.value_or(m_globalMode);

    m_globalModeOverride = windowMode;
    backWindow()->setWindowMode(unwrapped);
    for (auto &refWindow : m_refWindows)
    {
        refWindow->setWindowMode(unwrapped);
    }

    emit globalModeChanged(unwrapped);
}

void App::endGlobalModeOverride()
{
    startGlobalModeOverride(std::nullopt);
}

void App::saveSession()
{
    if (m_saveFilePath.isEmpty())
    {
        QString filePath = sessionSaving::showSaveAsDialog();
        if (filePath.isEmpty())
        {
            return;
        }
        m_saveFilePath = std::move(filePath);
    }
    sessionSaving::saveSession(m_saveFilePath);
}

void App::saveSessionAs()
{
    const QString filePath = sessionSaving::showSaveAsDialog(m_saveFilePath);
    if (filePath.isEmpty())
    {
        return;
    }
    sessionSaving::saveSession(m_saveFilePath);
}

void App::loadSession()
{
    const QString filepath = sessionSaving::showOpenDialog(m_saveFilePath);
    if (filepath.isEmpty())
    {
        return;
    }
    sessionSaving::loadSession(filepath);
}

void App::closeAllReferenceWindows()
{
    for (const auto &refWindow : m_refWindows)
    {
        refWindow->deleteLater();
    }
    m_refWindows.clear();
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

App::App(int &argc, char **argv, int flags)
    : QApplication(argc, argv, flags),
      m_preferences(Preferences::loadFromDisk(this)),
      m_globalMode(defaultWindowMode),
      m_backWindow(new BackWindow()),
      m_mainToolbar(new MainToolbar(m_backWindow))
{
    setApplicationName("Ghost Reference");
    loadStyleSheetFor(this);

    m_backWindow->show();

    positionToolBarDefault(m_mainToolbar);
    m_mainToolbar->show();

    ReferenceWindow *refWindow = newReferenceWindow();
    refWindow->show();

    const int timerIntervalMs = static_cast<int>(1000.0 / timerCallsPerSecond);
    m_timer = startTimer(timerIntervalMs);

    // const CmdParseResult parsedCmdArgs = parseCommandLine(this);
}

App::~App() = default;
