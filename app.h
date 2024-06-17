#pragma once

#include <optional>

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
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
    QPointer<BackWindow> m_backWindow = nullptr;
    MainToolbar *m_mainToolbar = nullptr;

    RefWindowList m_refWindows;
    ReferenceCollection m_referenceItems;

    WindowMode m_globalMode;
    std::optional<WindowMode> m_globalModeOverride;

    int m_timer = 0;
    QNetworkAccessManager *m_networkManager = nullptr;
    QScopedPointer<Preferences> m_preferences;

    QString m_saveFilePath;

public:
    static App *ghostRefInstance();

    App(int &argc, char **argv, int flags = ApplicationFlags);
    ~App() override;

    BackWindow *backWindow() const;
    MainToolbar *mainToolbar() const;

    WindowMode globalMode() const;
    void setGlobalMode(WindowMode mode);

    QNetworkAccessManager *networkManager();
    const Preferences *preferences() const;
    Preferences *preferences();

    const RefWindowList &referenceWindows() const;
    const ReferenceCollection &referenceItems() const;
    ReferenceCollection &referenceItems();

    ReferenceWindow *newReferenceWindow();

    void startGlobalModeOverride(std::optional<WindowMode> windowMode);
    void endGlobalModeOverride();

    void saveSession();
    void saveSessionAs();
    void loadSession();

    void closeAllReferenceWindows();

signals:
    void globalModeChanged(WindowMode newValue);

protected:
    void checkModifierKeyStates();
    void checkGhostStates();
    bool inOverrideMode() const;
    static bool isOverrideKeyHeld();

    void timerEvent(QTimerEvent *event) override;

private:
    typedef QMap<QString, ReferenceImageWP> ReferenceMap;
    static ReferenceMap &references();
    static QString uniqueReferenceName(const QString &basename);

    void cleanWindowList();
};

inline App *App::ghostRefInstance() { return qobject_cast<App *>(App::instance()); }

inline WindowMode App::globalMode() const { return m_globalMode; }

inline const Preferences *App::preferences() const { return m_preferences.get(); }

inline Preferences *App::preferences() { return m_preferences.get(); }

inline const App::RefWindowList &App::referenceWindows() const { return m_refWindows; }

inline const ReferenceCollection &App::referenceItems() const { return m_referenceItems; }

inline ReferenceCollection &App::referenceItems() { return m_referenceItems; }

inline bool App::inOverrideMode() const { return m_globalModeOverride.has_value(); }
inline BackWindow *App::backWindow() const { return m_backWindow; }
inline MainToolbar *App::mainToolbar() const { return m_mainToolbar; }
