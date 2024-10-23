#pragma once

#include <QtCore/QObject>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>

class BackWindow;

#define ACTION_DECL(name)                                                                                              \
private:                                                                                                               \
    QAction m_##name;                                                                                                  \
                                                                                                                       \
public:                                                                                                                \
    inline QAction &name()                                                                                             \
    {                                                                                                                  \
        return m_##name;                                                                                               \
    }

class BackWindowActions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BackWindowActions)

    ACTION_DECL(closeApplication);
    ACTION_DECL(openSession);
    ACTION_DECL(paste);
    ACTION_DECL(toggleAllRefsHidden);
    ACTION_DECL(toggleGhostMode);
    ACTION_DECL(toggleToolbar);
    ACTION_DECL(redo);
    ACTION_DECL(saveSession);
    ACTION_DECL(saveSessionAs);
    ACTION_DECL(showHelp);
    ACTION_DECL(showPreferences);
    ACTION_DECL(undo);

private:
    QActionGroup m_windowModeGroup;

public:
    explicit BackWindowActions(BackWindow *backWindow);
    ~BackWindowActions() override = default;

    BackWindow *backWindow() const;
};

#undef ACTION_DECL
