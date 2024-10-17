#pragma once

#include <optional>

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtGui/QCursor>
#include <QtWidgets/QApplication>

#include "reference_collection.h"
#include "types.h"

#include "widgets/back_window.h"

class QNetworkAccessManager;

class App : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(App)

public:
    typedef QList<QPointer<ReferenceWindow>> RefWindowList;

private:
    Preferences *m_preferences = nullptr;
    BackWindow *m_backWindow = nullptr;
    MainToolbar *m_mainToolbar = nullptr;
    SystemTrayIcon *m_systemTrayIcon = nullptr;

    RefWindowList m_refWindows;
    ReferenceCollection m_referenceItems;

    WindowMode m_globalMode;
    std::optional<WindowMode> m_globalModeOverride;

    int m_timer = 0;
    GlobalHotkeys *m_globalHotkeys = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    UndoStack *m_undoStack;

    bool m_allRefWindowsVisible = true;
    bool m_hasUnsavedChanges = false;

    // Used by setReferenceCursor
    struct
    {
        std::optional<QCursor> cursor;
        std::optional<RefType> refType;
    } m_referenceCursor;

    QString m_saveFilePath;

public:
    static App *ghostRefInstance();

    App(int &argc, char **argv, int flags = ApplicationFlags, const Preferences *prefs = nullptr);
    ~App() override;

    BackWindow *backWindow() const;
    MainToolbar *mainToolbar() const;

    // The global WindowMode not including any global mode override set by startGlobalModeOverride.
    WindowMode globalMode() const;
    void setGlobalMode(WindowMode mode);

    QNetworkAccessManager *networkManager();

    const Preferences *preferences() const;
    Preferences *preferences();
    // Replace the applications preferences with prefs. The ownership of prefs will be transferred.
    void setPreferences(Preferences *prefs);

    GlobalHotkeys *globalHotkeys() const;

    UndoStack *undoStack() const;

    const RefWindowList &referenceWindows() const;
    const ReferenceCollection &referenceItems() const;
    ReferenceCollection &referenceItems();

    ReferenceWindow *newReferenceWindow();
    ReferenceWindow *getReferenceWindow(RefWindowId identifier) const;

    // Sets the cursor used for widgets that display references e.g. PictureWidget.
    // The widgets will revert to their previous cursors when cursor is a null value.
    // If refType is given then only widgets for that type of reference are affected
    // (other widgets will use their default cursor).
    void setReferenceCursor(const std::optional<QCursor> &cursor, std::optional<RefType> refType = {});

    const QString &saveFilePath() const;

    // Save the current session. Returns true if saved successfully.
    bool saveSession();
    bool saveSessionAs();
    void loadSession();
    void loadSession(const QString &path);

    bool hasUnsavedChanges() const;
    void setUnsavedChanges(bool value = true);

    bool allRefWindowsVisible() const;
    void setAllRefWindowsVisible(bool value);

    void closeAllReferenceWindows();

    SystemTrayIcon *systemTrayIcon() const;
    void setSystemTrayIconVisible(bool value);

    void onStartUp();

signals:
    void allRefWindowsVisibleChanged(bool newValue);
    // Called whenever the global WindowMode changes including when startGlobalModeOverride
    // and endGlobalModeOverride are called.
    void globalModeChanged(WindowMode newValue);
    void preferencesReplaced(Preferences *prefs);
    void referenceCursorChanged(const std::optional<QCursor> &cursor, std::optional<RefType> refType);
    void referenceWindowAdded(ReferenceWindow *refWindow);

protected:
    void checkModifierKeyStates();
    void checkGhostStates();

    // Override the current WindowMode whilst isOverrideKeyHeld is True
    void startGlobalModeOverride(std::optional<WindowMode> windowMode);
    void endGlobalModeOverride();

    bool inOverrideMode() const;
    static bool isOverrideKeyHeld();

    bool event(QEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void cleanWindowList();
    void processCommandLineArgs();
    void refreshWindowName();
};

inline App *App::ghostRefInstance() { return qobject_cast<App *>(App::instance()); }

inline WindowMode App::globalMode() const { return m_globalMode; }

inline const Preferences *App::preferences() const
{
    return m_preferences;
}

inline Preferences *App::preferences()
{
    return m_preferences;
}

inline GlobalHotkeys *App::globalHotkeys() const
{
    return m_globalHotkeys;
}

inline UndoStack *App::undoStack() const
{
    return m_undoStack;
}

inline const App::RefWindowList &App::referenceWindows() const { return m_refWindows; }

inline const ReferenceCollection &App::referenceItems() const { return m_referenceItems; }

inline ReferenceCollection &App::referenceItems() { return m_referenceItems; }

inline const QString &App::saveFilePath() const
{
    return m_saveFilePath;
}

inline bool App::hasUnsavedChanges() const
{
    return m_hasUnsavedChanges;
}

inline bool App::allRefWindowsVisible() const
{
    return m_allRefWindowsVisible;
}

inline bool App::inOverrideMode() const { return m_globalModeOverride.has_value(); }
inline BackWindow *App::backWindow() const { return m_backWindow; }
inline MainToolbar *App::mainToolbar() const { return m_mainToolbar; }

inline SystemTrayIcon *App::systemTrayIcon() const
{
    return m_systemTrayIcon;
}